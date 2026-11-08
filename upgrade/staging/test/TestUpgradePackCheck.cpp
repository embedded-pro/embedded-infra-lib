#include "upgrade/staging/UpgradePackCheck.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace
{
    constexpr uint32_t applicationCapacity{ 2048 };
    constexpr const char* productName{ "test board" };
    constexpr uint16_t signatureLength{ 64 };
    constexpr uint32_t aesCounterSize{ 16 };

    template<std::size_t N>
    std::array<char, N> Text(const char* text)
    {
        std::array<char, N> result{};
        std::copy_n(text, std::min(std::strlen(text), N), result.begin());
        return result;
    }

    struct PackDescription
    {
        const char* productName{ ::productName };
        const char* productVersion{ "2.3.4" };
        const char* target{ "app" };
        uint16_t headerVersion{ 1 };
        uint32_t numberOfImages{ 1 };
        uint32_t imageSize{ 300 };
        std::array<uint8_t, 3> magic{ 'U', 'P', 'H' };
        uint32_t trailing{ 0 };
    };

    application::StagedPackHeader MakeHeader(const PackDescription& description)
    {
        application::StagedPackHeader header{};
        header.prologue = application::UpgradePackHeaderPrologue{ application::UpgradePackStatus::readyToDeploy, description.magic, 0, 0, 2, signatureLength };
        header.epilogue.headerVersion = description.headerVersion;
        header.epilogue.headerLength = sizeof(header.prologue) + signatureLength + sizeof(header.epilogue);
        header.epilogue.numberOfImages = description.numberOfImages;
        header.epilogue.productName = Text<64>(description.productName);
        header.epilogue.productVersion = Text<64>(description.productVersion);
        header.image.lengthOfHeaderAndImage = sizeof(header.image) + aesCounterSize + sizeof(application::ImageHeaderEpilogue) + description.imageSize;
        header.image.targetName = Text<8>(description.target);
        return header;
    }

    std::optional<application::UpgradePackVersion> Check(const PackDescription& description = {})
    {
        const auto header = MakeHeader(description);
        const auto packSize = header.epilogue.headerLength + header.image.lengthOfHeaderAndImage + description.trailing;
        return application::CheckStagedPackHeader(header, application::StagedPackLimits{ productName, "app", applicationCapacity, packSize });
    }

    std::optional<application::UpgradePackVersion> Parse(const char* text)
    {
        return application::ParseUpgradePackVersion(Text<64>(text));
    }

    struct HeaderDefect
    {
        const char* name;
        void (*apply)(PackDescription&);
    };

    class StagedPackHeaderDefectTest
        : public testing::TestWithParam<HeaderDefect>
    {};
}

TEST(UpgradePackCheckTest, a_pack_for_this_product_with_one_target_image_is_accepted_with_its_version)
{
    EXPECT_EQ((application::UpgradePackVersion{ 2, 3, 4 }), Check());
}

TEST(UpgradePackCheckTest, an_image_that_fills_the_slot_exactly_is_accepted)
{
    PackDescription description;
    description.imageSize = applicationCapacity;

    EXPECT_TRUE(Check(description).has_value());
}

TEST(UpgradePackCheckTest, the_epilogue_follows_the_prologue_and_its_signature)
{
    const auto header = MakeHeader({});

    EXPECT_EQ(sizeof(application::UpgradePackHeaderPrologue) + signatureLength, application::UpgradePackEpilogueOffset(header.prologue));
}

TEST(UpgradePackCheckTest, a_version_is_three_numbers_from_0_to_255)
{
    EXPECT_EQ((application::UpgradePackVersion{ 0, 0, 0 }), Parse("0.0.0"));
    EXPECT_EQ((application::UpgradePackVersion{ 255, 255, 255 }), Parse("255.255.255"));
    EXPECT_EQ((application::UpgradePackVersion{ 1, 10, 100 }), Parse("1.10.100"));
}

TEST(UpgradePackCheckTest, anything_else_is_not_a_version)
{
    EXPECT_FALSE(Parse("256.0.0").has_value());
    EXPECT_FALSE(Parse("2.3").has_value());
    EXPECT_FALSE(Parse("2.3.4.5").has_value());
    EXPECT_FALSE(Parse("2..4").has_value());
    EXPECT_FALSE(Parse("2.3.4-rc1").has_value());
    EXPECT_FALSE(Parse("").has_value());
}

TEST_P(StagedPackHeaderDefectTest, is_refused)
{
    PackDescription description;
    GetParam().apply(description);

    EXPECT_FALSE(Check(description).has_value());
}

INSTANTIATE_TEST_SUITE_P(Defects, StagedPackHeaderDefectTest,
    testing::Values(
        HeaderDefect{ "another_product", [](PackDescription& d)
            {
                d.productName = "another board";
            } },
        HeaderDefect{ "a_longer_product_name", [](PackDescription& d)
            {
                d.productName = "test board 2";
            } },
        HeaderDefect{ "bad_magic", [](PackDescription& d)
            {
                d.magic = { 'U', 'P', 'X' };
            } },
        HeaderDefect{ "header_version_2", [](PackDescription& d)
            {
                d.headerVersion = 2;
            } },
        HeaderDefect{ "two_images", [](PackDescription& d)
            {
                d.numberOfImages = 2;
            } },
        HeaderDefect{ "another_target", [](PackDescription& d)
            {
                d.target = "boot1st";
            } },
        HeaderDefect{ "an_image_larger_than_the_slot", [](PackDescription& d)
            {
                d.imageSize = applicationCapacity + 4;
            } },
        HeaderDefect{ "bytes_after_the_image", [](PackDescription& d)
            {
                d.trailing = 4;
            } },
        HeaderDefect{ "a_version_of_two_numbers", [](PackDescription& d)
            {
                d.productVersion = "2.3";
            } },
        HeaderDefect{ "an_empty_version", [](PackDescription& d)
            {
                d.productVersion = "";
            } },
        HeaderDefect{ "a_version_number_above_255", [](PackDescription& d)
            {
                d.productVersion = "2.256.4";
            } }),
    [](const testing::TestParamInfo<HeaderDefect>& info)
    {
        return std::string{ info.param.name };
    });
