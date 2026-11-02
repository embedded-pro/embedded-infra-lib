#include "application/bootloader_image/ElfSection.hpp"
#include "application/bootloader_image/test/ElfFile.hpp"
#include <gmock/gmock.h>
#include <variant>

namespace
{
    using namespace application;

    const ElfFile firmware{ {
        { ".text", 0x00000000, 0x00000000, std::vector<uint8_t>(256, 0x11) },
        { ".data", 0x20000000, 0x00000100, std::vector<uint8_t>(16, 0x22) },
        { ".bss", 0x20000010, 0x20000010, {}, 8 },
        { ".bootloader_keys", 0x00007fb0, 0x00007fb0, std::vector<uint8_t>(80, 0xff) },
    } };
}

TEST(TestElfSection, a_section_is_found_by_name_with_its_place_in_the_file_and_its_load_address)
{
    const auto bytes = firmware.Bytes();

    const auto found = FindElfSection(bytes, ".bootloader_keys");

    ASSERT_TRUE(std::holds_alternative<ElfSection>(found));
    const auto& section = std::get<ElfSection>(found);
    EXPECT_EQ(80u, section.size);
    EXPECT_EQ(0x00007fb0u, section.loadAddress);
    EXPECT_EQ(std::vector<uint8_t>(80, 0xff), std::vector<uint8_t>(bytes.begin() + section.fileOffset, bytes.begin() + section.fileOffset + section.size));
}

TEST(TestElfSection, the_load_address_is_where_the_section_lies_in_flash_not_where_it_runs)
{
    const auto found = FindElfSection(firmware.Bytes(), ".data");

    ASSERT_TRUE(std::holds_alternative<ElfSection>(found));
    EXPECT_EQ(0x00000100u, std::get<ElfSection>(found).loadAddress);
}

TEST(TestElfSection, an_unknown_section_is_reported)
{
    EXPECT_EQ(ElfError::sectionNotFound, std::get<ElfError>(FindElfSection(firmware.Bytes(), ".image_descriptor")));
}

TEST(TestElfSection, a_section_without_contents_in_the_file_is_reported)
{
    EXPECT_EQ(ElfError::sectionNotLoaded, std::get<ElfError>(FindElfSection(firmware.Bytes(), ".bss")));
}

TEST(TestElfSection, a_file_that_is_not_a_32_bit_little_endian_elf_is_reported)
{
    auto bytes = firmware.Bytes();
    bytes[4] = 2;

    EXPECT_EQ(ElfError::notElf, std::get<ElfError>(FindElfSection(bytes, ".text")));
    EXPECT_EQ(ElfError::notElf, std::get<ElfError>(FindElfSection(std::vector<uint8_t>(80, 0x7f), ".text")));
}

TEST(TestElfSection, a_truncated_file_is_reported)
{
    auto bytes = firmware.Bytes();
    bytes.resize(bytes.size() - 20);

    EXPECT_EQ(ElfError::malformed, std::get<ElfError>(FindElfSection(bytes, ".bootloader_keys")));
}
