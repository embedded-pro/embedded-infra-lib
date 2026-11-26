#include "hal/interfaces/Eeprom.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/Crc.hpp"
#include "services/util/NvmEepromRegion.hpp"
#include "services/util/NvmRecordStore.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using RestoreResult = services::NvmRecordStore::RestoreResult;
    using SaveResult = services::NvmRecordStore::SaveResult;

    constexpr uint32_t regionSize{ 16 };
    constexpr uint32_t storeSize{ 2 * regionSize };

    class SingleAccessEeprom
        : public hal::Eeprom
    {
    public:
        SingleAccessEeprom()
        {
            storage.fill(0xff);
        }

        uint32_t Size() const override
        {
            return static_cast<uint32_t>(storage.size());
        }

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override
        {
            Begin();
            ++writes;
            if (!dropWrites)
                std::ranges::copy(buffer, storage.begin() + address);
            Finish(onDone);
        }

        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override
        {
            Begin();
            std::ranges::copy_n(storage.begin() + address, static_cast<std::ptrdiff_t>(buffer.size()), buffer.begin());
            Finish(onDone);
        }

        void Erase(infra::Function<void()> onDone) override
        {
            Begin();
            storage.fill(0xff);
            Finish(onDone);
        }

        std::array<uint8_t, 2 * storeSize> storage{};
        bool overlapped{ false };
        bool dropWrites{ false };
        bool hold{ false };
        uint32_t writes{ 0 };

        void Complete()
        {
            hold = false;
            Schedule();
        }

    private:
        void Begin()
        {
            overlapped = overlapped || busy;
            busy = true;
        }

        void Finish(const infra::Function<void()>& onDone)
        {
            pending = onDone;
            if (!hold)
                Schedule();
        }

        void Schedule()
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    busy = false;
                    auto done = pending;
                    pending = nullptr;
                    done();
                });
        }

        bool busy{ false };
        infra::Function<void()> pending;
    };

    class ClientMock
        : public services::NvmRecordStore::Client
    {
    public:
        MOCK_METHOD(void, Restored, (infra::ConstByteRange payload), (override));
        MOCK_METHOD(std::size_t, Capture, (infra::ByteRange payload), (override));
        MOCK_METHOD(bool, Busy, (), (const, override));
    };

    auto Writes(std::vector<uint8_t> payload)
    {
        return [payload](infra::ByteRange buffer)
        {
            std::ranges::copy(payload, buffer.begin());
            return payload.size();
        };
    }

    MATCHER_P(PayloadIs, expected, "")
    {
        return std::vector<uint8_t>(arg.begin(), arg.end()) == expected;
    }

    void PutCopy(SingleAccessEeprom& eeprom, uint32_t address, uint16_t sequence, std::vector<uint8_t> payload)
    {
        std::vector<uint8_t> copy{ 0x52, 0x53, static_cast<uint8_t>(sequence), static_cast<uint8_t>(sequence >> 8), static_cast<uint8_t>(payload.size()), 0 };
        copy.insert(copy.end(), payload.begin(), payload.end());
        infra::Crc16CcittFalse crc;
        crc.Update(infra::MakeRange(copy));
        copy.insert(copy.begin() + 6, { static_cast<uint8_t>(crc.Result()), static_cast<uint8_t>(crc.Result() >> 8) });
        std::ranges::copy(copy, eeprom.storage.begin() + address);
    }

    class Store
    {
    public:
        Store(hal::Eeprom& eeprom, infra::ClaimableResource& access, uint32_t baseAddress, services::NvmRecordStore::Client& client)
            : first{ eeprom, baseAddress, regionSize }
            , second{ eeprom, baseAddress + regionSize, regionSize }
            , store{ first, second, access, client }
        {}

        services::NvmEepromRegion first;
        services::NvmEepromRegion second;
        services::NvmRecordStore::WithRegionSize<regionSize> store;
    };

    class NvmRecordStoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Start()
        {
            first.reset();
            second.reset();
            first.emplace(eeprom, access, 0, firstClient);
            second.emplace(eeprom, access, storeSize, secondClient);
            ExecuteAllActions();
        }

        void Save(std::vector<uint8_t> payload)
        {
            first->store.ScheduleSave();
            EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
            EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes(payload));
            ForwardTime(2s);
        }

        SingleAccessEeprom eeprom;
        infra::ClaimableResource access;
        testing::StrictMock<ClientMock> firstClient;
        testing::StrictMock<ClientMock> secondClient;
        std::optional<Store> first;
        std::optional<Store> second;
    };
}

TEST_F(NvmRecordStoreTest, restore_is_pending_until_the_copies_are_read)
{
    first.emplace(eeprom, access, 0, firstClient);

    EXPECT_EQ(RestoreResult::pending, first->store.Restore());
    EXPECT_EQ(SaveResult::none, first->store.LastSave());
}

TEST_F(NvmRecordStoreTest, erased_regions_report_defaults)
{
    Start();

    EXPECT_EQ(RestoreResult::defaults, first->store.Restore());
}

TEST_F(NvmRecordStoreTest, a_saved_payload_is_restored)
{
    Start();
    Save({ 1, 2, 3 });
    EXPECT_EQ(SaveResult::saved, first->store.LastSave());

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1, 2, 3 })));
    Start();
    EXPECT_EQ(RestoreResult::restored, first->store.Restore());
}

TEST_F(NvmRecordStoreTest, an_empty_payload_is_restored)
{
    Start();
    Save({});

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{})));
    Start();
}

TEST_F(NvmRecordStoreTest, the_newer_copy_is_restored)
{
    PutCopy(eeprom, 0, 4, { 1 });
    PutCopy(eeprom, regionSize, 5, { 2 });

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 2 })));
    Start();
}

TEST_F(NvmRecordStoreTest, the_newer_copy_is_found_across_a_sequence_wrap)
{
    PutCopy(eeprom, 0, 0, { 1 });
    PutCopy(eeprom, regionSize, 0xffff, { 2 });

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1 })));
    Start();
}

TEST_F(NvmRecordStoreTest, a_torn_newer_copy_falls_back_to_the_older_copy)
{
    PutCopy(eeprom, 0, 4, { 1 });
    PutCopy(eeprom, regionSize, 5, { 2 });
    eeprom.storage[regionSize + 8] ^= 0xff;

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1 })));
    Start();
}

TEST_F(NvmRecordStoreTest, unreadable_copies_report_corrupt)
{
    eeprom.storage[3] = 0;

    Start();

    EXPECT_EQ(RestoreResult::corrupt, first->store.Restore());
}

TEST_F(NvmRecordStoreTest, a_copy_with_a_length_beyond_the_region_is_rejected)
{
    PutCopy(eeprom, 0, 1, { 1 });
    eeprom.storage[4] = regionSize;

    Start();

    EXPECT_EQ(RestoreResult::corrupt, first->store.Restore());
}

TEST_F(NvmRecordStoreTest, a_save_waits_for_the_delay)
{
    Start();
    first->store.ScheduleSave();

    ForwardTime(1s);
    EXPECT_EQ(SaveResult::pending, first->store.LastSave());
    EXPECT_EQ(0, eeprom.writes);
}

TEST_F(NvmRecordStoreTest, a_burst_of_requests_makes_one_save)
{
    Start();
    first->store.ScheduleSave();
    ForwardTime(1s);
    first->store.ScheduleSave();
    ForwardTime(1s);

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    ForwardTime(1s);

    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
}

TEST_F(NvmRecordStoreTest, saves_alternate_between_the_copies)
{
    Start();
    Save({ 1 });
    Save({ 2 });

    EXPECT_EQ(1, eeprom.storage[2]);
    EXPECT_EQ(2, eeprom.storage[regionSize + 2]);
}

TEST_F(NvmRecordStoreTest, a_save_after_a_restore_overwrites_the_older_copy)
{
    PutCopy(eeprom, 0, 4, { 1 });
    PutCopy(eeprom, regionSize, 3, { 2 });
    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1 })));
    Start();

    Save({ 3 });

    EXPECT_EQ(5, eeprom.storage[regionSize + 2]);
    EXPECT_EQ(4, eeprom.storage[2]);
}

TEST_F(NvmRecordStoreTest, a_busy_client_defers_the_save)
{
    Start();
    first->store.ScheduleSave();

    EXPECT_CALL(firstClient, Busy()).WillOnce(testing::Return(true));
    ForwardTime(2s);
    EXPECT_EQ(SaveResult::pending, first->store.LastSave());

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    ForwardTime(2s);
    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
}

TEST_F(NvmRecordStoreTest, a_save_that_does_not_read_back_fails)
{
    Start();
    eeprom.dropWrites = true;

    Save({ 1 });

    EXPECT_EQ(SaveResult::failed, first->store.LastSave());
}

TEST_F(NvmRecordStoreTest, after_a_failed_save_the_next_save_writes_the_same_copy)
{
    Start();
    eeprom.dropWrites = true;
    Save({ 1 });
    eeprom.dropWrites = false;

    Save({ 2 });

    EXPECT_EQ(1, eeprom.storage[2]);
    EXPECT_EQ(0xff, eeprom.storage[regionSize]);
}

TEST_F(NvmRecordStoreTest, a_request_during_a_save_starts_another_save)
{
    Start();
    first->store.ScheduleSave();
    EXPECT_CALL(firstClient, Busy()).Times(4).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(testing::Invoke([this](infra::ByteRange buffer)
                                                               {
                                                                   first->store.ScheduleSave();
                                                                   buffer[0] = 1;
                                                                   return std::size_t{ 1 };
                                                               }))
        .WillOnce(Writes({ 2 }));
    ForwardTime(2s);
    EXPECT_EQ(SaveResult::pending, first->store.LastSave());

    ForwardTime(2s);
    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
    EXPECT_EQ(2, eeprom.storage[regionSize + 8]);
}

TEST_F(NvmRecordStoreTest, two_stores_restore_from_one_eeprom_one_at_a_time)
{
    Start();

    EXPECT_FALSE(eeprom.overlapped);
    EXPECT_EQ(RestoreResult::defaults, first->store.Restore());
    EXPECT_EQ(RestoreResult::defaults, second->store.Restore());
}

TEST_F(NvmRecordStoreTest, two_stores_save_to_one_eeprom_one_at_a_time_and_restore_their_own_payload)
{
    Start();
    first->store.ScheduleSave();
    second->store.ScheduleSave();

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(secondClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    EXPECT_CALL(secondClient, Capture(testing::_)).WillOnce(Writes({ 2 }));
    ForwardTime(2s);

    EXPECT_FALSE(eeprom.overlapped);
    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
    EXPECT_EQ(SaveResult::saved, second->store.LastSave());

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1 })));
    EXPECT_CALL(secondClient, Restored(PayloadIs(std::vector<uint8_t>{ 2 })));
    Start();
}

TEST_F(NvmRecordStoreTest, a_client_that_becomes_busy_while_waiting_for_access_does_not_save)
{
    Start();
    first->store.ScheduleSave();
    second->store.ScheduleSave();

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(secondClient, Busy()).WillOnce(testing::Return(false)).WillOnce(testing::Return(true));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    ForwardTime(2s);

    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
    EXPECT_EQ(SaveResult::pending, second->store.LastSave());
}

TEST_F(NvmRecordStoreTest, a_failed_save_releases_access_for_the_other_store)
{
    Start();
    eeprom.dropWrites = true;
    first->store.ScheduleSave();
    second->store.ScheduleSave();

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(secondClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    EXPECT_CALL(secondClient, Capture(testing::_)).WillOnce(Writes({ 2 }));
    ForwardTime(2s);

    EXPECT_EQ(SaveResult::failed, first->store.LastSave());
    EXPECT_EQ(SaveResult::failed, second->store.LastSave());
    EXPECT_FALSE(eeprom.overlapped);
}

TEST_F(NvmRecordStoreTest, a_store_without_shared_access_saves_and_restores)
{
    services::NvmEepromRegion firstRegion{ eeprom, 0, regionSize };
    services::NvmEepromRegion secondRegion{ eeprom, regionSize, regionSize };
    std::optional<services::NvmRecordStore::WithRegionSize<regionSize>> store;
    store.emplace(firstRegion, secondRegion, firstClient);
    ExecuteAllActions();
    store->ScheduleSave();
    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 7 }));
    ForwardTime(2s);

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 7 })));
    store.reset();
    store.emplace(firstRegion, secondRegion, firstClient);
    ExecuteAllActions();
}

TEST_F(NvmRecordStoreTest, a_payload_that_fills_the_region_is_restored)
{
    Start();
    Save({ 1, 2, 3, 4, 5, 6, 7, 8 });

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6, 7, 8 })));
    Start();
}

TEST_F(NvmRecordStoreTest, a_save_waits_for_the_restore)
{
    eeprom.hold = true;
    first.emplace(eeprom, access, 0, firstClient);
    ExecuteAllActions();
    first->store.ScheduleSave();

    ForwardTime(2s);
    EXPECT_EQ(SaveResult::pending, first->store.LastSave());

    eeprom.Complete();
    ExecuteAllActions();
    EXPECT_EQ(RestoreResult::defaults, first->store.Restore());

    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    ForwardTime(2s);
    EXPECT_EQ(SaveResult::saved, first->store.LastSave());
}

TEST_F(NvmRecordStoreTest, a_save_after_the_sequence_wraps_is_restored)
{
    PutCopy(eeprom, 0, 0xfffe, { 1 });
    PutCopy(eeprom, regionSize, 0xffff, { 2 });
    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 2 })));
    Start();

    Save({ 3 });
    EXPECT_EQ(0, eeprom.storage[2]);
    EXPECT_EQ(0, eeprom.storage[3]);

    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 3 })));
    Start();
}

TEST_F(NvmRecordStoreTest, a_save_after_restoring_the_second_copy_writes_the_first_copy)
{
    PutCopy(eeprom, regionSize, 4, { 2 });
    EXPECT_CALL(firstClient, Restored(PayloadIs(std::vector<uint8_t>{ 2 })));
    Start();

    Save({ 3 });

    EXPECT_EQ(5, eeprom.storage[2]);
    EXPECT_EQ(3, eeprom.storage[8]);
}

TEST_F(NvmRecordStoreTest, a_busy_client_after_access_is_granted_does_not_save_twice)
{
    Start();
    first->store.ScheduleSave();
    second->store.ScheduleSave();
    EXPECT_CALL(firstClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(secondClient, Busy()).WillOnce(testing::Return(false)).WillOnce(testing::Return(true));
    EXPECT_CALL(firstClient, Capture(testing::_)).WillOnce(Writes({ 1 }));
    ForwardTime(2s);

    EXPECT_CALL(secondClient, Busy()).Times(2).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(secondClient, Capture(testing::_)).WillOnce(Writes({ 2 }));
    ForwardTime(2s);
    EXPECT_EQ(SaveResult::saved, second->store.LastSave());

    ForwardTime(10s);
}
