#include "bootloader/StatusJournal.hpp"
#include "bootloader/test/FlashFake.hpp"
#include "infra/util/ByteRange.hpp"
#include <gmock/gmock.h>

namespace
{
    using application::UpgradePackStatus;

    constexpr uint32_t regionSize = 4 * bootloader::FlashFake::programmingUnit;
    constexpr uint32_t newest = bootloader::StatusJournalEntry::firstGeneration;

    class FlashFakeWithResetOnErase
        : public bootloader::FlashFake
    {
    public:
        using bootloader::FlashFake::FlashFake;

        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override
        {}
    };

    class TestStatusJournal
        : public ::testing::Test
    {
    public:
        void LoadEntry(bootloader::FlashFake& target, UpgradePackStatus status, uint32_t generation, uint32_t address)
        {
            const auto entry = bootloader::MakeStatusJournalEntry(status, 0, generation);
            const auto bytes = infra::MakeByteRange(entry);
            target.Load(std::vector<uint8_t>(bytes.begin(), bytes.end()), address);
        }

        void FillFirstRegion(bootloader::FlashFake& target, UpgradePackStatus status, uint32_t generation)
        {
            for (uint32_t address = 0; address != regionSize; address += bootloader::FlashFake::programmingUnit)
                LoadEntry(target, status, generation, address);
        }

        bootloader::FlashFake flash{ 2, regionSize };
        bootloader::StatusJournal journal{ flash };
    };
}

TEST_F(TestStatusJournal, an_erased_journal_reads_empty_without_an_error)
{
    EXPECT_EQ(UpgradePackStatus::empty, journal.Status());
    EXPECT_EQ(0u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, a_recorded_status_reads_back)
{
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
}

TEST_F(TestStatusJournal, the_latest_record_wins)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    journal.Record(UpgradePackStatus::readyToDeploy, 0);
    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_EQ(UpgradePackStatus::deployStarted, journal.Status());
}

TEST_F(TestStatusJournal, an_error_code_reads_back_with_its_status)
{
    journal.Record(UpgradePackStatus::invalid, 1000);

    EXPECT_EQ(UpgradePackStatus::invalid, journal.Status());
    EXPECT_EQ(1000u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, each_record_takes_a_fresh_programming_unit)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_TRUE(flash.programmed[1]);
    EXPECT_FALSE(flash.programmed[2]);
}

TEST_F(TestStatusJournal, recording_the_current_status_again_writes_nothing)
{
    journal.Record(UpgradePackStatus::deployStarted, 0);

    journal.Record(UpgradePackStatus::deployStarted, 0);
    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_FALSE(flash.programmed[1]);
}

TEST_F(TestStatusJournal, the_same_status_with_another_error_is_written)
{
    journal.Record(UpgradePackStatus::invalid, 0);

    journal.Record(UpgradePackStatus::invalid, 1000);

    EXPECT_TRUE(flash.programmed[1]);
    EXPECT_EQ(1000u, journal.ErrorCode());
}

TEST_F(TestStatusJournal, clearing_returns_to_empty)
{
    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    journal.Clear();

    EXPECT_EQ(UpgradePackStatus::empty, journal.Status());
}

TEST_F(TestStatusJournal, a_torn_entry_is_ignored_and_skipped)
{
    journal.Record(UpgradePackStatus::downloaded, 0);
    flash.Load({ 0x45, 0x46, 0x4a, 0x31, 0xfc }, bootloader::FlashFake::programmingUnit);

    EXPECT_EQ(UpgradePackStatus::downloaded, journal.Status());

    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
    EXPECT_TRUE(flash.programmed[2]);
}

TEST_F(TestStatusJournal, a_full_region_continues_in_the_other_region_and_erases_the_full_one)
{
    for (auto i = 0; i != 4; ++i)
        journal.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, 0);

    journal.Record(UpgradePackStatus::readyToDeploy, 0);

    EXPECT_EQ(1u, flash.erasures);
    EXPECT_EQ(UpgradePackStatus::readyToDeploy, journal.Status());
    EXPECT_FALSE(flash.programmed[0]);
    EXPECT_TRUE(flash.programmed[4]);
    EXPECT_FALSE(flash.programmed[5]);
}

TEST_F(TestStatusJournal, a_region_is_used_again_after_the_other_region_is_full)
{
    for (auto i = 0; i != 9; ++i)
        journal.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, i);

    EXPECT_EQ(2u, flash.erasures);
    EXPECT_EQ(UpgradePackStatus::downloaded, journal.Status());
    EXPECT_EQ(8u, journal.ErrorCode());
    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_FALSE(flash.programmed[4]);
}

TEST_F(TestStatusJournal, a_reset_before_the_full_region_is_erased_keeps_the_new_status)
{
    FlashFakeWithResetOnErase resetting{ 2, regionSize };
    bootloader::StatusJournal interrupted{ resetting };
    for (auto i = 0; i != 4; ++i)
        interrupted.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, 0);

    interrupted.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_EQ(UpgradePackStatus::deployStarted, bootloader::StatusJournal{ resetting }.Status());
}

TEST_F(TestStatusJournal, the_region_with_the_lower_generation_is_current)
{
    LoadEntry(flash, UpgradePackStatus::deployStarted, newest - 1, 0);
    LoadEntry(flash, UpgradePackStatus::readyToDeploy, newest, regionSize);

    EXPECT_EQ(UpgradePackStatus::deployStarted, journal.Status());
}

TEST_F(TestStatusJournal, entries_left_in_the_old_region_are_ignored)
{
    FillFirstRegion(flash, UpgradePackStatus::readyToDeploy, newest - 2);
    LoadEntry(flash, UpgradePackStatus::deployStarted, newest - 3, regionSize);
    LoadEntry(flash, UpgradePackStatus::deployed, newest - 3, regionSize + bootloader::FlashFake::programmingUnit);

    EXPECT_EQ(UpgradePackStatus::deployed, journal.Status());
}

TEST_F(TestStatusJournal, a_rollover_erases_leftovers_in_the_other_region_before_it_writes)
{
    FillFirstRegion(flash, UpgradePackStatus::readyToDeploy, newest - 1);
    flash.Load({ 0x00, 0x12 }, regionSize + bootloader::FlashFake::programmingUnit);

    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_EQ(2u, flash.erasures);
    EXPECT_EQ(UpgradePackStatus::deployStarted, journal.Status());
}

TEST_F(TestStatusJournal, an_exhausted_generation_restarts_the_whole_journal)
{
    FillFirstRegion(flash, UpgradePackStatus::readyToDeploy, 0);

    journal.Record(UpgradePackStatus::deployStarted, 0);

    EXPECT_EQ(UpgradePackStatus::deployStarted, journal.Status());
    EXPECT_TRUE(flash.programmed[0]);
    EXPECT_FALSE(flash.programmed[1]);
}

TEST_F(TestStatusJournal, a_region_over_several_sectors_uses_all_of_them)
{
    bootloader::FlashFake large{ 4, 2 * bootloader::FlashFake::programmingUnit };
    bootloader::StatusJournal spread{ large };

    for (auto i = 0; i != 3; ++i)
        spread.Record(i % 2 == 0 ? UpgradePackStatus::downloaded : UpgradePackStatus::invalid, 0);
    spread.Record(UpgradePackStatus::deployed, 0);

    EXPECT_EQ(0u, large.erasures);
    EXPECT_EQ(UpgradePackStatus::deployed, spread.Status());
}
