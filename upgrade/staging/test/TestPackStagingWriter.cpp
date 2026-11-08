#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "upgrade/staging/PackStagingWriter.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace
{
    constexpr uint16_t pageSize{ 8 };

    class PackStagingWriterTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        PackStagingWriterTest()
        {
            for (auto& sector : flash.sectors)
                sector.assign(sector.size(), 0);
        }

        std::optional<bool> Erase()
        {
            std::optional<bool> result;
            writer.Erase([&result](bool ok)
                {
                    result = ok;
                });
            ExecuteAllActions();
            return result;
        }

        std::optional<bool> Append(infra::ConstByteRange data)
        {
            std::optional<bool> result;
            writer.Append(data, [&result](bool ok)
                {
                    result = ok;
                });
            ExecuteAllActions();
            return result;
        }

        std::optional<bool> Flush()
        {
            std::optional<bool> result;
            writer.Flush([&result](bool ok)
                {
                    result = ok;
                });
            ExecuteAllActions();
            return result;
        }

        bool SectorHolds(const std::array<uint8_t, 16>& expected) const
        {
            return std::equal(expected.begin(), expected.end(), flash.sectors[0].begin(), flash.sectors[0].end());
        }

        hal::FlashStub flash{ 2, 16 };
        std::array<uint8_t, 8> scratch{};
        bool proceed{ true };
        application::PackStagingWriter writer{ flash, pageSize, infra::MakeByteRange(scratch), [this]()
            {
                return proceed;
            } };
    };
}

TEST_F(PackStagingWriterTest, erase_blanks_every_sector_and_checks_it)
{
    EXPECT_EQ(true, Erase());
    EXPECT_EQ(0u, writer.Written());
    EXPECT_EQ(0xff, flash.sectors[0][0]);
    EXPECT_EQ(0xff, flash.sectors[1][15]);
}

TEST_F(PackStagingWriterTest, a_sector_that_does_not_read_back_blank_fails_the_erase)
{
    flash.delaySignalEraseDone = true;
    std::optional<bool> result;
    writer.Erase([&result](bool ok)
        {
            result = ok;
        });

    flash.sectors[0][3] = 0;
    flash.onEraseDone();
    ExecuteAllActions();

    EXPECT_EQ(false, result);
}

TEST_F(PackStagingWriterTest, data_short_of_a_page_is_held_until_the_page_fills)
{
    Erase();
    const std::array<uint8_t, 4> data{ 1, 2, 3, 4 };

    EXPECT_EQ(true, Append(infra::MakeConstByteRange(data)));
    EXPECT_TRUE(writer.HoldsPartialPage());
    EXPECT_EQ(4u, writer.Written());
    EXPECT_EQ(0xff, flash.sectors[0][0]);
}

TEST_F(PackStagingWriterTest, a_full_page_is_programmed_and_verified)
{
    Erase();
    const std::array<uint8_t, 4> first{ 1, 2, 3, 4 };
    const std::array<uint8_t, 4> second{ 5, 6, 7, 8 };
    Append(infra::MakeConstByteRange(first));

    EXPECT_EQ(true, Append(infra::MakeConstByteRange(second)));
    EXPECT_FALSE(writer.HoldsPartialPage());
    EXPECT_TRUE(SectorHolds({ 1, 2, 3, 4, 5, 6, 7, 8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }));
}

TEST_F(PackStagingWriterTest, bytes_past_the_page_boundary_start_the_next_page)
{
    Erase();
    const std::array<uint8_t, 6> first{ 1, 2, 3, 4, 5, 6 };
    const std::array<uint8_t, 4> second{ 7, 8, 9, 10 };
    Append(infra::MakeConstByteRange(first));
    Append(infra::MakeConstByteRange(second));

    EXPECT_TRUE(writer.HoldsPartialPage());
    EXPECT_EQ(true, Flush());
    EXPECT_TRUE(SectorHolds({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }));
    EXPECT_EQ(10u, writer.Written());
}

TEST_F(PackStagingWriterTest, a_page_that_does_not_read_back_fails_the_append)
{
    Erase();
    flash.stopAfterWriteSteps = 0;
    const std::array<uint8_t, 8> data{ 1, 2, 3, 4, 5, 6, 7, 8 };

    EXPECT_EQ(false, Append(infra::MakeConstByteRange(data)));
}

TEST_F(PackStagingWriterTest, a_step_that_may_not_proceed_ends_without_a_result)
{
    proceed = false;

    EXPECT_EQ(std::nullopt, Erase());
    EXPECT_FALSE(writer.Busy());
}

TEST_F(PackStagingWriterTest, the_writer_is_busy_while_the_flash_works)
{
    writer.Erase([](bool) {});

    EXPECT_TRUE(writer.Busy());
    ExecuteAllActions();
    EXPECT_FALSE(writer.Busy());
}
