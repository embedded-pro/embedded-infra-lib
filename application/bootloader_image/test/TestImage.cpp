#include "application/bootloader_image/Image.hpp"
#include "application/bootloader_image/test/HexRecord.hpp"
#include "bootloader/ApplicationCheck.hpp"
#include <cstring>
#include <gmock/gmock.h>
#include <variant>

namespace
{
    using namespace application;
    using testing::ElementsAre;

    std::vector<uint8_t> Bytes(std::size_t count, uint8_t value)
    {
        return std::vector<uint8_t>(count, value);
    }
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

TEST(TestPatchHex, bytes_inside_one_data_record_are_replaced_and_its_checksum_follows)
{
    std::vector<std::string> lines{ HexRecord(0, 4, { 0, 0 }), HexRecord(0x7fb0, 0, Bytes(16, 0xff)), ":00000001FF" };

    EXPECT_TRUE(PatchHex(lines, 0x7fb4, std::vector<uint8_t>{ 1, 2, 3 }));

    EXPECT_THAT(lines, ElementsAre(HexRecord(0, 4, { 0, 0 }), HexRecord(0x7fb0, 0, { 0xff, 0xff, 0xff, 0xff, 1, 2, 3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), ":00000001FF"));
}

TEST(TestPatchHex, bytes_spanning_records_are_replaced_in_each)
{
    std::vector<std::string> lines{ HexRecord(0x0ff8, 0, Bytes(8, 0xaa)), HexRecord(0x1000, 0, Bytes(8, 0xbb)) };

    EXPECT_TRUE(PatchHex(lines, 0x0ffe, std::vector<uint8_t>{ 1, 2, 3, 4 }));

    EXPECT_THAT(lines, ElementsAre(HexRecord(0x0ff8, 0, { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 1, 2 }), HexRecord(0x1000, 0, { 3, 4, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb })));
}

TEST(TestPatchHex, extended_linear_and_segment_addresses_are_followed)
{
    std::vector<std::string> lines{ HexRecord(0, 4, { 0x00, 0x01 }), HexRecord(0x0010, 0, Bytes(4, 0)), HexRecord(0, 2, { 0x20, 0x00 }), HexRecord(0x0010, 0, Bytes(4, 0)) };

    EXPECT_TRUE(PatchHex(lines, 0x00010010, std::vector<uint8_t>{ 7 }));
    EXPECT_TRUE(PatchHex(lines, 0x00020013, std::vector<uint8_t>{ 9 }));

    EXPECT_EQ(HexRecord(0x0010, 0, { 7, 0, 0, 0 }), lines[1]);
    EXPECT_EQ(HexRecord(0x0010, 0, { 0, 0, 0, 9 }), lines[3]);
}

TEST(TestPatchHex, nothing_changes_unless_every_byte_lies_in_a_data_record)
{
    const std::vector<std::string> original{ HexRecord(0x7fb0, 0, Bytes(16, 0xff)), ":00000001FF" };
    auto lines = original;

    EXPECT_FALSE(PatchHex(lines, 0x7fbc, std::vector<uint8_t>(8, 1)));
    EXPECT_EQ(original, lines);
}

TEST(TestPatchHex, a_line_that_is_not_a_valid_record_refuses_the_patch)
{
    std::vector<std::string> lines{ HexRecord(0x7fb0, 0, Bytes(4, 0xff)), ":04000000FFFFFFFF01" };

    EXPECT_FALSE(PatchHex(lines, 0x7fb0, std::vector<uint8_t>{ 1 }));
}
