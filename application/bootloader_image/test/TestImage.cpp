#include "application/bootloader_image/Image.hpp"
#include "bootloader/ApplicationCheck.hpp"
#include <cstring>
#include <gmock/gmock.h>
#include <variant>

namespace
{
    using namespace application;
    using testing::ElementsAre;
}

TEST(TestImageDescriptor, the_descriptor_is_the_libraries_for_the_image)
{
    const std::vector<uint8_t> image(5000, 0x5a);
    const auto expected = bootloader::MakeImageDescriptor(infra::MakeRange(image));

    const auto descriptor = ImageDescriptorFor(image);

    ASSERT_EQ(bootloader::imageDescriptorSize, descriptor.size());
    EXPECT_EQ(0, std::memcmp(&expected, descriptor.data(), descriptor.size()));
}

TEST(TestCombine, the_bootloader_comes_first_with_its_start_address_and_one_end_record)
{
    const std::vector<std::string> bootloader{ ":020000040000FA", ":10000000000004200100000000000000000000005B", ":0400000300000475802", ":00000001FF" };
    const std::vector<std::string> application{ ":020000040000FA", ":10800000000004204186000000000000000000003F", ":040000030000864132", ":00000001FF" };

    const auto combined = Combine(bootloader, application);

    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(combined));
    EXPECT_THAT(std::get<std::vector<std::string>>(combined), ElementsAre(":020000040000FA", ":10000000000004200100000000000000000000005B", ":0400000300000475802",
                                                                  ":020000020000FC", ":020000040000FA", ":020000040000FA", ":10800000000004204186000000000000000000003F", ":00000001FF"));
}

TEST(TestCombine, blank_lines_are_dropped)
{
    const auto combined = Combine({ ":00000001FF", "" }, { "  ", ":00000001FF" });

    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(combined));
    EXPECT_THAT(std::get<std::vector<std::string>>(combined), ElementsAre(":020000020000FC", ":020000040000FA", ":00000001FF"));
}

TEST(TestCombine, a_line_that_is_not_a_hex_record_is_named)
{
    const auto combined = Combine({ ":00000001FF" }, { "garbage" });

    ASSERT_TRUE(std::holds_alternative<CombineError>(combined));
    EXPECT_EQ(CombineError::Source::application, std::get<CombineError>(combined).source);
    EXPECT_EQ(1u, std::get<CombineError>(combined).line);
}
