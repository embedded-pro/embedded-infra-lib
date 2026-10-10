#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include <array>
#include <cstdint>
#include <vector>

namespace
{
    class FlashStubTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        hal::FlashStub flash{ 3, 4 };
    };
}

TEST_F(FlashStubTest, write_programs_bytes_as_bitwise_and)
{
    std::array<uint8_t, 2> first{ 0xf0, 0x0f };
    std::array<uint8_t, 2> second{ 0x3c, 0xff };

    flash.WriteBuffer(infra::MakeRange(first), 1, [] {});
    flash.WriteBuffer(infra::MakeRange(second), 1, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::vector<uint8_t>{ 0xff, 0x30, 0x0f, 0xff }), flash.sectors[0]);
}

TEST_F(FlashStubTest, write_across_sector_boundaries_programs_every_sector)
{
    std::array<uint8_t, 6> data{ 1, 2, 3, 4, 5, 6 };

    flash.WriteBuffer(infra::MakeRange(data), 3, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::vector<uint8_t>{ 0xff, 0xff, 0xff, 1 }), flash.sectors[0]);
    EXPECT_EQ((std::vector<uint8_t>{ 2, 3, 4, 5 }), flash.sectors[1]);
    EXPECT_EQ((std::vector<uint8_t>{ 6, 0xff, 0xff, 0xff }), flash.sectors[2]);
}

TEST_F(FlashStubTest, write_into_sectors_of_different_sizes_follows_the_sector_layout)
{
    flash.sectors[0].resize(2, 0xff);
    std::array<uint8_t, 3> data{ 1, 2, 3 };

    flash.WriteBuffer(infra::MakeRange(data), 1, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::vector<uint8_t>{ 0xff, 1 }), flash.sectors[0]);
    EXPECT_EQ((std::vector<uint8_t>{ 2, 3, 0xff, 0xff }), flash.sectors[1]);
}

TEST_F(FlashStubTest, read_across_sector_boundaries_reads_every_sector)
{
    flash.sectors[0] = { 1, 2, 3, 4 };
    flash.sectors[1] = { 5, 6, 7, 8 };
    flash.sectors[2] = { 9, 10, 11, 12 };
    std::array<uint8_t, 6> data{};

    flash.ReadBuffer(infra::MakeRange(data), 3, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 6>{ 4, 5, 6, 7, 8, 9 }), data);
}

TEST_F(FlashStubTest, read_past_the_end_of_flash_continues_at_the_start)
{
    flash.sectors[0] = { 1, 2, 3, 4 };
    flash.sectors[2] = { 9, 10, 11, 12 };
    std::array<uint8_t, 4> data{};

    flash.ReadBuffer(infra::MakeRange(data), 10, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 4>{ 11, 12, 1, 2 }), data);
}

TEST_F(FlashStubTest, write_and_read_complete_through_the_event_dispatcher)
{
    infra::MockCallback<void()> writeDone;
    infra::MockCallback<void()> readDone;
    std::array<uint8_t, 2> data{};

    flash.WriteBuffer(infra::MakeRange(data), 0, [&writeDone]
        {
            writeDone.callback();
        });
    flash.ReadBuffer(infra::MakeRange(data), 0, [&readDone]
        {
            readDone.callback();
        });

    EXPECT_CALL(writeDone, callback());
    EXPECT_CALL(readDone, callback());
    ExecuteAllActions();
}

TEST_F(FlashStubTest, writes_after_the_configured_write_steps_are_ignored_but_completed)
{
    infra::MockCallback<void()> done;
    std::array<uint8_t, 1> data{ 0x00 };
    flash.stopAfterWriteSteps = 1;

    flash.WriteBuffer(infra::MakeRange(data), 0, [] {});
    flash.WriteBuffer(infra::MakeRange(data), 1, [&done]
        {
            done.callback();
        });

    EXPECT_CALL(done, callback());
    ExecuteAllActions();
    EXPECT_EQ((std::vector<uint8_t>{ 0x00, 0xff, 0xff, 0xff }), flash.sectors[0]);
}

TEST_F(FlashStubTest, erase_sets_the_erased_sectors_to_ff)
{
    flash.sectors[0] = { 1, 2, 3, 4 };
    flash.sectors[1] = { 5, 6, 7, 8 };
    flash.sectors[2] = { 9, 10, 11, 12 };

    flash.EraseSectors(1, 3, [] {});
    ExecuteAllActions();

    EXPECT_EQ((std::vector<uint8_t>{ 1, 2, 3, 4 }), flash.sectors[0]);
    EXPECT_EQ((std::vector<uint8_t>{ 0xff, 0xff, 0xff, 0xff }), flash.sectors[1]);
    EXPECT_EQ((std::vector<uint8_t>{ 0xff, 0xff, 0xff, 0xff }), flash.sectors[2]);
}
