#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Crc.hpp"
#include "upgrade/staging/StagedPackReader.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>

namespace
{
    constexpr uint16_t signatureLength{ 8 };

    class StagedPackReaderTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        StagedPackReaderTest()
        {
            for (std::size_t i = 0; i != flash.sectors[0].size(); ++i)
                flash.sectors[0][i] = static_cast<uint8_t>(i * 7 + 3);
        }

        uint32_t CrcOf(uint32_t size) const
        {
            infra::Crc32 crc;
            crc.Update(infra::ConstByteRange{ flash.sectors[0].data(), flash.sectors[0].data() + size });
            return crc.Result();
        }

        template<class T>
        void Place(const T& value, uint32_t offset)
        {
            std::memcpy(flash.sectors[0].data() + offset, &value, sizeof(T));
        }

        application::StagedPackHeader WriteHeader()
        {
            application::StagedPackHeader header{};
            header.prologue.magic = application::upgradePackMagic;
            header.prologue.signatureLength = signatureLength;
            header.epilogue.headerVersion = 1;
            header.epilogue.headerLength = sizeof(header.prologue) + signatureLength + sizeof(header.epilogue);
            header.epilogue.numberOfImages = 1;
            header.image.lengthOfHeaderAndImage = 1234;
            header.image.targetName = { 'a', 'p', 'p' };

            Place(header.prologue, 0);
            Place(header.epilogue, application::UpgradePackEpilogueOffset(header.prologue));
            Place(header.image, header.epilogue.headerLength);
            return header;
        }

        std::optional<bool> CheckCrc(uint32_t size, uint32_t expected)
        {
            std::optional<bool> result;
            reader.CheckCrc(size, expected, [&result](bool ok)
                {
                    result = ok;
                });
            ExecuteAllActions();
            return result;
        }

        std::optional<application::StagedPackHeader> ReadHeader(uint32_t limit)
        {
            std::optional<application::StagedPackHeader> result;
            reader.ReadHeader(limit, [&result](const application::StagedPackHeader& header)
                {
                    result = header;
                });
            ExecuteAllActions();
            return result;
        }

        hal::FlashStub flash{ 1, 512 };
        std::array<uint8_t, 16> scratch{};
        bool proceed{ true };
        application::StagedPackReader reader{ flash, infra::MakeByteRange(scratch), [this]()
            {
                return proceed;
            } };
    };
}

TEST_F(StagedPackReaderTest, a_matching_crc_over_several_chunks_is_reported)
{
    EXPECT_EQ(true, CheckCrc(100, CrcOf(100)));
}

TEST_F(StagedPackReaderTest, a_different_crc_is_reported)
{
    EXPECT_EQ(false, CheckCrc(100, CrcOf(100) ^ 1));
}

TEST_F(StagedPackReaderTest, the_header_is_read_from_its_three_places)
{
    const auto written = WriteHeader();

    const auto header = ReadHeader(512);

    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(written.prologue.signatureLength, header->prologue.signatureLength);
    EXPECT_EQ(written.epilogue.headerLength, header->epilogue.headerLength);
    EXPECT_EQ(written.image.lengthOfHeaderAndImage, header->image.lengthOfHeaderAndImage);
}

TEST_F(StagedPackReaderTest, an_epilogue_past_the_limit_is_left_unread)
{
    WriteHeader();

    const auto header = ReadHeader(sizeof(application::UpgradePackHeaderPrologue));

    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(0u, header->epilogue.headerLength);
    EXPECT_EQ(0u, header->image.lengthOfHeaderAndImage);
}

TEST_F(StagedPackReaderTest, an_image_prologue_past_the_limit_is_left_unread)
{
    const auto written = WriteHeader();

    const auto header = ReadHeader(written.epilogue.headerLength);

    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(written.epilogue.headerLength, header->epilogue.headerLength);
    EXPECT_EQ(0u, header->image.lengthOfHeaderAndImage);
}

TEST_F(StagedPackReaderTest, a_step_that_may_not_proceed_ends_without_a_result)
{
    WriteHeader();
    proceed = false;

    EXPECT_EQ(std::nullopt, CheckCrc(100, CrcOf(100)));
    EXPECT_EQ(std::nullopt, ReadHeader(512));
    EXPECT_FALSE(reader.Busy());
}

TEST_F(StagedPackReaderTest, the_reader_is_busy_while_the_flash_works)
{
    reader.CheckCrc(32, 0, [](bool) {});

    EXPECT_TRUE(reader.Busy());
    ExecuteAllActions();
    EXPECT_FALSE(reader.Busy());
}
