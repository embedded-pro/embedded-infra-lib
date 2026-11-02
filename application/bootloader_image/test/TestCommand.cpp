#include "application/bootloader_image/Command.hpp"
#include "application/bootloader_image/ElfSection.hpp"
#include "application/bootloader_image/Image.hpp"
#include "application/bootloader_image/KeySet.hpp"
#include "application/bootloader_image/Pack.hpp"
#include "application/bootloader_image/test/ElfFile.hpp"
#include "application/bootloader_image/test/HexRecord.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <iterator>
#include <sstream>
#include <variant>

namespace
{
    using namespace application;
    using testing::HasSubstr;

    class TestCommand
        : public ::testing::Test
    {
    public:
        TestCommand()
        {
            std::filesystem::create_directories(directory);
        }

        ~TestCommand() override
        {
            std::filesystem::remove_all(directory);
        }

        int Run(std::vector<std::string> arguments)
        {
            return application::Run(arguments, out, err, random);
        }

        std::string Path(const char* name) const
        {
            return (directory / name).string();
        }

        static std::vector<uint8_t> Read(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        static void Write(const std::string& path, const std::vector<uint8_t>& bytes)
        {
            std::ofstream file{ path, std::ios::binary };
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        static void WriteText(const std::string& path, const std::string& text)
        {
            std::ofstream{ path, std::ios::binary } << text;
        }

        static std::string ReadText(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        std::vector<uint8_t> GenerateKeySlot()
        {
            EXPECT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
            const auto slot = BootloaderKeySlot(std::get<KeySet>(ParseKeySet(Read(Path("keys.bin")))));
            return { slot.begin(), slot.end() };
        }

        static std::vector<uint8_t> Erased(std::size_t size)
        {
            return std::vector<uint8_t>(size, 0xff);
        }

        static std::string HexOf(uint32_t address, const std::vector<uint8_t>& bytes)
        {
            std::string text = HexRecord(0, 4, { static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16) }) + "\n";
            for (std::size_t offset = 0; offset < bytes.size(); offset += 16)
            {
                const auto end = std::min(bytes.size(), offset + 16);
                text += HexRecord(static_cast<uint16_t>(address + offset), 0, { bytes.begin() + offset, bytes.begin() + end }) + "\n";
            }
            return text + ":00000001FF\n";
        }

        std::filesystem::path directory{ std::filesystem::temp_directory_path() / ("bootloader_image_" + std::string{ ::testing::UnitTest::GetInstance()->current_test_info()->name() }) };
        std::ostringstream out;
        std::ostringstream err;
        uint32_t counter{ 0 };
        RandomSource random{ [this](std::span<uint8_t> bytes)
            {
                for (auto& byte : bytes)
                    byte = static_cast<uint8_t>(++counter * 53);
                return true;
            } };
    };
}

TEST_F(TestCommand, help_lists_every_command)
{
    EXPECT_EQ(0, Run({ "--help" }));
    EXPECT_THAT(out.str(), HasSubstr("generate-keys"));
    EXPECT_THAT(out.str(), HasSubstr("bootloader-keys"));
    EXPECT_THAT(out.str(), HasSubstr("descriptor"));
    EXPECT_THAT(out.str(), HasSubstr("combine"));
    EXPECT_THAT(out.str(), HasSubstr("inject-keys"));
    EXPECT_THAT(out.str(), HasSubstr("pack"));
}

TEST_F(TestCommand, no_command_shows_the_usage_and_fails)
{
    EXPECT_EQ(1, Run({}));
    EXPECT_THAT(err.str(), HasSubstr("Usage"));
}

TEST_F(TestCommand, an_unknown_command_is_named_and_fails)
{
    EXPECT_EQ(1, Run({ "make-coffee" }));
    EXPECT_THAT(err.str(), HasSubstr("make-coffee"));
}

TEST_F(TestCommand, generate_keys_writes_a_key_set)
{
    EXPECT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_TRUE(std::holds_alternative<KeySet>(ParseKeySet(Read(Path("keys.bin")))));
    EXPECT_THAT(out.str(), HasSubstr("secret"));
}

TEST_F(TestCommand, generate_keys_never_overwrites_a_file)
{
    Write(Path("keys.bin"), { 1, 2, 3 });

    EXPECT_EQ(1, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ((std::vector<uint8_t>{ 1, 2, 3 }), Read(Path("keys.bin")));
    EXPECT_THAT(err.str(), HasSubstr("already exists"));
}

TEST_F(TestCommand, generate_keys_needs_an_output)
{
    EXPECT_EQ(1, Run({ "generate-keys" }));
    EXPECT_THAT(err.str(), HasSubstr("--output"));
}

TEST_F(TestCommand, bootloader_keys_writes_the_key_slot_of_a_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ(0, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    const auto keys = std::get<KeySet>(ParseKeySet(Read(Path("keys.bin"))));
    const auto slot = BootloaderKeySlot(keys);
    EXPECT_EQ(std::vector<uint8_t>(slot.begin(), slot.end()), Read(Path("slot.bin")));
}

TEST_F(TestCommand, bootloader_keys_refuses_a_damaged_key_set_and_writes_nothing)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    auto file = Read(Path("keys.bin"));
    file[20] ^= 0x01;
    Write(Path("keys.bin"), file);

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    EXPECT_FALSE(std::filesystem::exists(Path("slot.bin")));
    EXPECT_THAT(err.str(), HasSubstr("damaged"));
}

TEST_F(TestCommand, bootloader_keys_names_a_file_that_is_not_a_key_set)
{
    Write(Path("keys.bin"), { 1, 2, 3 });

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("not a key set"));
}

TEST_F(TestCommand, bootloader_keys_reports_a_missing_key_set)
{
    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("absent.bin"), "--output", Path("slot.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("Cannot read"));
}

TEST_F(TestCommand, descriptor_writes_the_image_descriptor_of_an_image)
{
    Write(Path("image.bin"), std::vector<uint8_t>(3000, 0x11));

    EXPECT_EQ(0, Run({ "descriptor", "--image", Path("image.bin"), "--output", Path("descriptor.bin") }));

    EXPECT_EQ(ImageDescriptorFor(Read(Path("image.bin"))), Read(Path("descriptor.bin")));
}

TEST_F(TestCommand, descriptor_reports_a_missing_image)
{
    EXPECT_EQ(1, Run({ "descriptor", "--image", Path("absent.bin"), "--output", Path("descriptor.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("Cannot read"));
}

TEST_F(TestCommand, combine_writes_one_hex_file)
{
    WriteText(Path("bootloader.hex"), ":10000000000004200100000000000000000000005B\n:00000001FF\n");
    WriteText(Path("application.hex"), ":10800000000004204186000000000000000000003F\n:00000001FF\n");

    EXPECT_EQ(0, Run({ "combine", "--output", Path("combined.hex"), Path("bootloader.hex"), Path("application.hex") }));

    EXPECT_EQ(":10000000000004200100000000000000000000005B\n:020000020000FC\n:020000040000FA\n:10800000000004204186000000000000000000003F\n:00000001FF\n", ReadText(Path("combined.hex")));
}

TEST_F(TestCommand, combine_names_the_file_and_line_of_a_bad_record)
{
    WriteText(Path("bootloader.hex"), ":00000001FF\n");
    WriteText(Path("application.hex"), ":00000001FF\nnot a record\n");

    EXPECT_EQ(1, Run({ "combine", "--output", Path("combined.hex"), Path("bootloader.hex"), Path("application.hex") }));

    EXPECT_THAT(err.str(), HasSubstr(Path("application.hex") + " line 2"));
    EXPECT_FALSE(std::filesystem::exists(Path("combined.hex")));
}

TEST_F(TestCommand, combine_needs_two_hex_files)
{
    EXPECT_EQ(1, Run({ "combine", "--output", Path("combined.hex"), Path("bootloader.hex") }));

    EXPECT_THAT(err.str(), HasSubstr("combine needs"));
}

#ifndef _WIN32
TEST_F(TestCommand, a_key_set_is_readable_by_its_owner_only)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    using std::filesystem::perms;
    EXPECT_EQ(perms::owner_read | perms::owner_write, std::filesystem::status(Path("keys.bin")).permissions() & perms::all);
}

TEST_F(TestCommand, a_key_slot_is_readable_by_its_owner_only_even_when_it_replaces_a_file)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    Write(Path("slot.bin"), { 1, 2, 3 });
    std::filesystem::permissions(Path("slot.bin"), std::filesystem::perms::all);

    ASSERT_EQ(0, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("slot.bin") }));

    using std::filesystem::perms;
    EXPECT_EQ(perms::owner_read | perms::owner_write, std::filesystem::status(Path("slot.bin")).permissions() & perms::all);
    EXPECT_EQ(bootloaderKeySlotSize, Read(Path("slot.bin")).size());
}

TEST_F(TestCommand, bootloader_keys_refuses_an_output_linked_to_the_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    std::filesystem::create_hard_link(Path("keys.bin"), Path("link.bin"));

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("link.bin") }));

    EXPECT_EQ(keySetFileSize, Read(Path("keys.bin")).size());
}
#endif

TEST_F(TestCommand, bootloader_keys_refuses_to_write_over_its_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("keys.bin") }));

    EXPECT_EQ(keySetFileSize, Read(Path("keys.bin")).size());
    EXPECT_THAT(err.str(), HasSubstr("the key set itself"));
}

TEST_F(TestCommand, a_file_that_cannot_be_written_is_reported)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));

    EXPECT_EQ(1, Run({ "bootloader-keys", "--keys", Path("keys.bin"), "--output", Path("missing/slot.bin") }));

    EXPECT_THAT(err.str(), HasSubstr("Cannot write"));
}

TEST_F(TestCommand, descriptor_refuses_to_write_over_its_image)
{
    Write(Path("image.bin"), std::vector<uint8_t>(3000, 0x11));

    EXPECT_EQ(1, Run({ "descriptor", "--image", Path("image.bin"), "--output", Path("image.bin") }));

    EXPECT_EQ(3000u, Read(Path("image.bin")).size());
}

TEST_F(TestCommand, combine_refuses_to_write_over_an_input)
{
    WriteText(Path("bootloader.hex"), ":00000001FF\n");
    WriteText(Path("application.hex"), ":00000001FF\n");

    EXPECT_EQ(1, Run({ "combine", "--output", Path("application.hex"), Path("bootloader.hex"), Path("application.hex") }));

    EXPECT_EQ(":00000001FF\n", ReadText(Path("application.hex")));
}

namespace
{
    const std::vector<uint8_t> code(64, 0x5a);

    ElfFile Bootloader(std::size_t slotSize = 80, const char* slotName = ".bootloader_keys")
    {
        return ElfFile{ { { ".text", 0x00000000, 0x00000000, code }, { slotName, 0x00007fb0, 0x00007fb0, std::vector<uint8_t>(slotSize, 0xff) } } };
    }
}

TEST_F(TestCommand, inject_keys_writes_the_key_slot_into_the_elf_and_every_hex_file)
{
    const auto slot = GenerateKeySlot();
    Write(Path("boot.elf"), Bootloader().Bytes());
    WriteText(Path("boot.hex"), HexOf(0, code) + HexOf(0x7fb0, Erased(80)));
    WriteText(Path("combined.hex"), HexOf(0x7fb0, Erased(80)) + HexOf(0x8000, code));

    EXPECT_EQ(0, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf"), "--hex", Path("boot.hex"), "--hex", Path("combined.hex") }));

    const auto elf = Read(Path("boot.elf"));
    const auto section = std::get<ElfSection>(FindElfSection(elf, ".bootloader_keys"));
    EXPECT_EQ(slot, std::vector<uint8_t>(elf.begin() + section.fileOffset, elf.begin() + section.fileOffset + section.size));
    EXPECT_EQ(HexOf(0, code) + HexOf(0x7fb0, slot), ReadText(Path("boot.hex")));
    EXPECT_EQ(HexOf(0x7fb0, slot) + HexOf(0x8000, code), ReadText(Path("combined.hex")));
}

TEST_F(TestCommand, inject_keys_takes_another_section_name)
{
    const auto slot = GenerateKeySlot();
    Write(Path("boot.elf"), Bootloader(80, ".keys").Bytes());

    EXPECT_EQ(0, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf"), "--section", ".keys" }));

    const auto elf = Read(Path("boot.elf"));
    const auto section = std::get<ElfSection>(FindElfSection(elf, ".keys"));
    EXPECT_EQ(slot, std::vector<uint8_t>(elf.begin() + section.fileOffset, elf.begin() + section.fileOffset + section.size));
}

TEST_F(TestCommand, inject_keys_refuses_a_section_that_is_not_a_key_slot_and_changes_nothing)
{
    GenerateKeySlot();
    const auto original = Bootloader(64).Bytes();
    Write(Path("boot.elf"), original);

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf") }));

    EXPECT_EQ(original, Read(Path("boot.elf")));
    EXPECT_THAT(err.str(), HasSubstr("64 bytes"));
}

TEST_F(TestCommand, inject_keys_names_an_elf_without_the_section)
{
    GenerateKeySlot();
    Write(Path("boot.elf"), Bootloader(80, ".other").Bytes());

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf") }));
    EXPECT_THAT(err.str(), HasSubstr(".bootloader_keys"));
}

TEST_F(TestCommand, inject_keys_changes_no_file_when_a_hex_file_lacks_the_slot)
{
    GenerateKeySlot();
    const auto originalElf = Bootloader().Bytes();
    const auto completeHex = HexOf(0x7fb0, Erased(80));
    const auto partialHex = HexOf(0x7fb0, Erased(48));
    Write(Path("boot.elf"), originalElf);
    WriteText(Path("complete.hex"), completeHex);
    WriteText(Path("partial.hex"), partialHex);

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf"), "--hex", Path("complete.hex"), "--hex", Path("partial.hex") }));

    EXPECT_EQ(originalElf, Read(Path("boot.elf")));
    EXPECT_EQ(completeHex, ReadText(Path("complete.hex")));
    EXPECT_EQ(partialHex, ReadText(Path("partial.hex")));
    EXPECT_THAT(err.str(), HasSubstr("partial.hex"));
}

TEST_F(TestCommand, inject_keys_refuses_a_damaged_key_set)
{
    GenerateKeySlot();
    auto keys = Read(Path("keys.bin"));
    keys[20] ^= 0x01;
    Write(Path("keys.bin"), keys);
    const auto original = Bootloader().Bytes();
    Write(Path("boot.elf"), original);

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf") }));

    EXPECT_EQ(original, Read(Path("boot.elf")));
    EXPECT_THAT(err.str(), HasSubstr("damaged"));
}

TEST_F(TestCommand, inject_keys_names_a_file_that_is_not_an_elf)
{
    GenerateKeySlot();
    Write(Path("boot.elf"), { 1, 2, 3 });

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin"), "--elf", Path("boot.elf") }));
    EXPECT_THAT(err.str(), HasSubstr("not a 32-bit"));
}

TEST_F(TestCommand, inject_keys_needs_a_key_set_and_an_elf)
{
    EXPECT_EQ(1, Run({ "inject-keys", "--elf", Path("boot.elf") }));
    EXPECT_THAT(err.str(), HasSubstr("--keys"));

    EXPECT_EQ(1, Run({ "inject-keys", "--keys", Path("keys.bin") }));
    EXPECT_THAT(err.str(), HasSubstr("--elf"));
}

namespace
{
    std::string PackText(const std::vector<uint8_t>& pack, std::size_t offset, std::size_t size)
    {
        if (pack.size() < offset + size)
            return {};
        const auto* begin = reinterpret_cast<const char*>(pack.data() + offset);
        return std::string{ begin, strnlen(begin, size) };
    }

    constexpr std::size_t productNameOffset = 16 + 64 + 8;
    constexpr std::size_t productVersionOffset = productNameOffset + 64;
    constexpr std::size_t componentNameOffset = productVersionOffset + 64;
}

TEST_F(TestCommand, pack_writes_a_pack_for_the_product_signed_with_the_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    Write(Path("image.bin"), std::vector<uint8_t>(3000, 0x5a));

    EXPECT_EQ(0, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("image.bin"), "--destination", "0x8000", "--product", "test board", "--product-version", "1.7.0", "--output", Path("pack.bin") }));

    const auto pack = Read(Path("pack.bin"));
    EXPECT_EQ("test board", PackText(pack, productNameOffset, 64));
    EXPECT_EQ("1.7.0", PackText(pack, productVersionOffset, 64));
    EXPECT_EQ("application", PackText(pack, componentNameOffset, 64));
    EXPECT_GT(pack.size(), 3000u);
}

TEST_F(TestCommand, pack_takes_a_decimal_destination_and_a_component_name)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    Write(Path("image.bin"), { 1, 2, 3, 4 });

    EXPECT_EQ(0, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("image.bin"), "--destination", "32768", "--product", "p", "--component", "motor", "--output", Path("pack.bin") }));
    EXPECT_EQ("motor", PackText(Read(Path("pack.bin")), componentNameOffset, 64));
}

TEST_F(TestCommand, pack_refuses_a_destination_that_is_not_an_address)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    Write(Path("image.bin"), { 1, 2, 3, 4 });

    EXPECT_EQ(1, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("image.bin"), "--destination", "slot", "--product", "p", "--output", Path("pack.bin") }));
    EXPECT_THAT(err.str(), HasSubstr("slot"));
    EXPECT_FALSE(std::filesystem::exists(Path("pack.bin")));
}

TEST_F(TestCommand, pack_refuses_a_name_that_does_not_fit_the_pack)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    Write(Path("image.bin"), { 1, 2, 3, 4 });

    EXPECT_EQ(1, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("image.bin"), "--destination", "0x8000", "--product", std::string(64, 'x'), "--output", Path("pack.bin") }));
    EXPECT_THAT(err.str(), HasSubstr("63"));
}

TEST_F(TestCommand, pack_reports_a_missing_image_and_a_damaged_key_set)
{
    ASSERT_EQ(0, Run({ "generate-keys", "--output", Path("keys.bin") }));
    EXPECT_EQ(1, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("missing.bin"), "--destination", "0x8000", "--product", "p", "--output", Path("pack.bin") }));
    EXPECT_THAT(err.str(), HasSubstr("missing.bin"));

    Write(Path("image.bin"), { 1, 2, 3, 4 });
    Write(Path("keys.bin"), { 1, 2, 3 });
    EXPECT_EQ(1, Run({ "pack", "--keys", Path("keys.bin"), "--image", Path("image.bin"), "--destination", "0x8000", "--product", "p", "--output", Path("pack.bin") }));
    EXPECT_THAT(err.str(), HasSubstr("not a key set"));
}

TEST_F(TestCommand, pack_needs_its_options)
{
    EXPECT_EQ(1, Run({ "pack", "--image", "i", "--destination", "0", "--product", "p", "--output", "o" }));
    EXPECT_THAT(err.str(), HasSubstr("--keys"));
    EXPECT_EQ(1, Run({ "pack", "--keys", "k", "--destination", "0", "--product", "p", "--output", "o" }));
    EXPECT_THAT(err.str(), HasSubstr("--image"));
    EXPECT_EQ(1, Run({ "pack", "--keys", "k", "--image", "i", "--product", "p", "--output", "o" }));
    EXPECT_THAT(err.str(), HasSubstr("--destination"));
    EXPECT_EQ(1, Run({ "pack", "--keys", "k", "--image", "i", "--destination", "0", "--output", "o" }));
    EXPECT_THAT(err.str(), HasSubstr("--product"));
    EXPECT_EQ(1, Run({ "pack", "--keys", "k", "--image", "i", "--destination", "0", "--product", "p" }));
    EXPECT_THAT(err.str(), HasSubstr("--output"));
}
