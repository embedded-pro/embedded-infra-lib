#include "application/bootloader_image/Command.hpp"
#include "application/bootloader_image/ElfSection.hpp"
#include "application/bootloader_image/Image.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace application
{
    namespace
    {
        constexpr const char* usage = R"(Usage: application.bootloader_image <command> [options]

Commands:
  generate-keys --output <file>
      Write a new key set: an AES-128 image key and an ECDSA-P256 key pair, 112 bytes.
      Never overwrites an existing file.
  bootloader-keys --keys <key set> --output <file>
      Write the bootloader's 80-byte key slot (public key, then image key) from a key set.
  descriptor --image <binary> --output <file>
      Write the 64-byte image descriptor (size and SHA-256) of an application image.
  combine --output <file> <bootloader hex> <application hex>
      Join the bootloader's and the application's Intel HEX files into one, the bootloader first.
  inject-keys --keys <key set> --elf <bootloader elf> [--hex <file>]... [--section <name>]
      Write the key slot into a linked bootloader: into its section (default .bootloader_keys) of the
      ELF file, and at that section's address into every Intel HEX file given, such as the bootloader's
      own and a combined one. No file changes unless all of them take the slot.

Key sets, the key slot and the image descriptor: docs/Bootloader.md in embedded-infra-lib.
)";

        constexpr int success = 0;
        constexpr int failure = 1;

        std::optional<std::string> Option(const std::vector<std::string>& arguments, std::string_view name)
        {
            for (std::size_t index = 1; index + 1 < arguments.size(); ++index)
                if (arguments[index] == name)
                    return arguments[index + 1];
            return std::nullopt;
        }

        std::vector<std::string> Options(const std::vector<std::string>& arguments, std::string_view name)
        {
            std::vector<std::string> values;
            for (std::size_t index = 1; index + 1 < arguments.size(); ++index)
                if (arguments[index] == name)
                    values.push_back(arguments[++index]);
            return values;
        }

        std::optional<std::vector<uint8_t>> ReadFile(std::string_view path)
        {
            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };
            if (!file)
                return std::nullopt;
            return std::vector<uint8_t>{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        enum class Create
        {
            newOnly,
            replacing,
        };

        enum class Written
        {
            ok,
            exists,
            failed,
        };

#ifndef _WIN32
        bool WriteAll(int descriptor, std::span<const uint8_t> bytes)
        {
            for (std::size_t done = 0; done < bytes.size();)
            {
                const auto count = ::write(descriptor, bytes.data() + done, bytes.size() - done);
                if (count <= 0)
                    return false;
                done += static_cast<std::size_t>(count);
            }
            return true;
        }

        // Key material is owner-only from the moment the file exists, whatever the umask; newOnly is atomic
        Written WriteSecret(const std::string& path, std::span<const uint8_t> bytes, Create create)
        {
            const auto flags = O_WRONLY | O_CREAT | (create == Create::newOnly ? O_EXCL : O_TRUNC);
            const auto descriptor = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR);
            if (descriptor < 0)
                return errno == EEXIST ? Written::exists : Written::failed;

            const auto written = ::fchmod(descriptor, S_IRUSR | S_IWUSR) == 0 && WriteAll(descriptor, bytes);
            return ::close(descriptor) == 0 && written ? Written::ok : Written::failed;
        }
#else
        Written WriteSecret(const std::string& path, std::span<const uint8_t> bytes, Create create)
        {
            if (create == Create::newOnly && std::filesystem::exists(path))
                return Written::exists;

            std::ofstream file{ path, std::ios::binary | std::ios::trunc };
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            file.close();
            if (file.fail())
                return Written::failed;

            std::error_code error;
            std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, error);
            return error ? Written::failed : Written::ok;
        }
#endif

        bool WriteFile(std::string_view path, std::span<const uint8_t> bytes)
        {
            std::ofstream file{ std::filesystem::path{ path }, std::ios::binary | std::ios::trunc };
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            file.close();
            return !file.fail();
        }

        bool SameFile(std::string_view first, std::string_view second)
        {
            std::error_code error;
            return std::filesystem::equivalent(std::filesystem::path{ first }, std::filesystem::path{ second }, error);
        }

        int AlreadyExists(std::ostream& err, std::string_view path)
        {
            err << path << " already exists. A key set is never overwritten: choose another file name, or delete the old one if nothing still needs it.\n";
            return failure;
        }

        std::optional<std::vector<std::string>> ReadLines(std::string_view path)
        {
            std::ifstream file{ std::filesystem::path{ path } };
            if (!file)
                return std::nullopt;
            std::vector<std::string> lines;
            for (std::string line; std::getline(file, line);)
                lines.push_back(line);
            return lines;
        }

        std::vector<std::string> Positional(const std::vector<std::string>& arguments)
        {
            std::vector<std::string> positional;
            for (std::size_t index = 1; index < arguments.size(); ++index)
            {
                if (arguments[index].starts_with("--"))
                    ++index;
                else
                    positional.push_back(arguments[index]);
            }
            return positional;
        }

        int Missing(std::ostream& err, std::string_view command, std::string_view option)
        {
            err << command << " needs " << option << ".\n\n"
                << usage;
            return failure;
        }

        int GenerateKeys(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random)
        {
            const auto output = Option(arguments, "--output");
            if (!output)
                return Missing(err, arguments[0], "--output <file>");

            if (std::filesystem::exists(*output))
                return AlreadyExists(err, *output);

            const auto keys = GenerateKeySet(random);
            if (!keys)
            {
                err << "The random source failed; no key set was written.\n";
                return failure;
            }

            const auto written = WriteSecret(*output, Serialize(*keys), Create::newOnly);
            if (written == Written::exists)
                return AlreadyExists(err, *output);
            if (written == Written::failed)
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": new key set written. Keep it secret: it signs the packs your bootloaders accept.\n";
            return success;
        }

        int ReportKeySetError(std::ostream& err, std::string_view path, std::size_t size, KeySetError error)
        {
            if (error == KeySetError::wrongSize)
                err << path << " is not a key set: it holds " << size << " bytes, a key set holds " << keySetFileSize << ".\n";
            else
                err << path << " is damaged: its public key is not the one its private key gives. Restore it from the repository secret.\n";
            return failure;
        }

        std::optional<std::array<uint8_t, bootloaderKeySlotSize>> LoadKeySlot(std::string_view path, std::ostream& err)
        {
            const auto file = ReadFile(path);
            if (!file)
            {
                err << "Cannot read the key set " << path << ".\n";
                return std::nullopt;
            }

            const auto parsed = ParseKeySet(*file);
            if (const auto* error = std::get_if<KeySetError>(&parsed))
            {
                ReportKeySetError(err, path, file->size(), *error);
                return std::nullopt;
            }

            return BootloaderKeySlot(std::get<KeySet>(parsed));
        }

        int BootloaderKeys(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err)
        {
            const auto keysPath = Option(arguments, "--keys");
            const auto output = Option(arguments, "--output");
            if (!keysPath)
                return Missing(err, arguments[0], "--keys <key set>");
            if (!output)
                return Missing(err, arguments[0], "--output <file>");

            if (SameFile(*keysPath, *output))
            {
                err << "--output is the key set itself: writing the slot there would destroy the key set. Choose another output file.\n";
                return failure;
            }

            const auto slot = LoadKeySlot(*keysPath, err);
            if (!slot)
                return failure;

            if (WriteSecret(*output, *slot, Create::replacing) != Written::ok)
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": bootloader key slot written from " << *keysPath << ".\n";
            return success;
        }
    }

    namespace
    {
        int Descriptor(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err)
        {
            const auto imagePath = Option(arguments, "--image");
            const auto output = Option(arguments, "--output");
            if (!imagePath)
                return Missing(err, arguments[0], "--image <binary>");
            if (!output)
                return Missing(err, arguments[0], "--output <file>");

            if (SameFile(*imagePath, *output))
            {
                err << "--output is the image itself; choose another output file.\n";
                return failure;
            }

            const auto image = ReadFile(*imagePath);
            if (!image)
            {
                err << "Cannot read the image " << *imagePath << ".\n";
                return failure;
            }

            if (!WriteFile(*output, ImageDescriptorFor(*image)))
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": image descriptor for " << image->size() << " bytes of " << *imagePath << ".\n";
            return success;
        }

        int ReportCombineError(std::ostream& err, const std::vector<std::string>& files, const CombineError& error)
        {
            const auto& file = error.source == CombineError::Source::bootloader ? files[0] : files[1];
            err << file << " line " << error.line << " is not an Intel HEX record; nothing was written.\n";
            return failure;
        }

        int CombineImages(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err)
        {
            const auto output = Option(arguments, "--output");
            const auto files = Positional(arguments);
            if (!output || files.size() != 2)
                return Missing(err, arguments[0], "--output <file>, then the bootloader's and the application's hex files");

            if (SameFile(*output, files[0]) || SameFile(*output, files[1]))
            {
                err << "--output is one of the hex files being combined; choose another output file.\n";
                return failure;
            }

            const auto bootloader = ReadLines(files[0]);
            const auto application = ReadLines(files[1]);
            if (!bootloader || !application)
            {
                err << "Cannot read " << (bootloader ? files[1] : files[0]) << ".\n";
                return failure;
            }

            const auto combined = Combine(*bootloader, *application);
            if (const auto* error = std::get_if<CombineError>(&combined))
                return ReportCombineError(err, files, *error);

            std::ofstream file{ *output, std::ios::binary | std::ios::trunc };
            for (const auto& line : std::get<std::vector<std::string>>(combined))
                file << line << '\n';
            file.close();
            if (file.fail())
            {
                err << "Cannot write " << *output << ".\n";
                return failure;
            }

            out << *output << ": " << files[0] << " and " << files[1] << " combined.\n";
            return success;
        }
    }

    namespace
    {
        constexpr std::string_view keySlotSection{ ".bootloader_keys" };

        void ReportElfError(std::ostream& err, std::string_view path, std::string_view section, ElfError error)
        {
            switch (error)
            {
                case ElfError::notElf:
                    err << path << " is not a 32-bit little-endian ELF file.\n";
                    break;
                case ElfError::malformed:
                    err << path << " is a damaged ELF file: its headers point outside the file.\n";
                    break;
                case ElfError::sectionNotFound:
                    err << path << " has no section " << section << ": link the bootloader with its key slot there, or name the slot's section with --section.\n";
                    break;
                case ElfError::sectionNotLoaded:
                    err << path << ": section " << section << " has no contents in flash to hold the key slot.\n";
                    break;
            }
        }

        std::optional<ElfSection> LocateKeySlot(std::span<const uint8_t> elf, std::string_view path, std::string_view section, std::ostream& err)
        {
            const auto found = FindElfSection(elf, section);
            if (const auto* error = std::get_if<ElfError>(&found))
            {
                ReportElfError(err, path, section, *error);
                return std::nullopt;
            }

            const auto& slot = std::get<ElfSection>(found);
            if (slot.size != bootloaderKeySlotSize)
            {
                err << path << ": section " << section << " is " << slot.size << " bytes; a key slot is " << bootloaderKeySlotSize << ".\n";
                return std::nullopt;
            }
            return slot;
        }

        using HexFile = std::pair<std::string, std::vector<std::string>>;

        std::optional<std::vector<HexFile>> PatchedHexFiles(const std::vector<std::string>& paths, uint32_t address, std::span<const uint8_t> slot, std::ostream& err)
        {
            std::vector<HexFile> files;
            for (const auto& path : paths)
            {
                auto lines = ReadLines(path);
                if (!lines || !PatchHex(*lines, address, slot))
                {
                    err << path << " holds no Intel HEX data for all " << slot.size() << " bytes of the key slot at 0x" << std::hex << address << std::dec << "; no file was changed.\n";
                    return std::nullopt;
                }
                files.emplace_back(path, std::move(*lines));
            }
            return files;
        }

        bool WriteLines(const std::string& path, const std::vector<std::string>& lines)
        {
            std::ofstream file{ std::filesystem::path{ path }, std::ios::binary | std::ios::trunc };
            for (const auto& line : lines)
                file << line << '\n';
            file.close();
            return !file.fail();
        }

        bool WriteAllFiles(std::string_view elfPath, std::span<const uint8_t> elf, const std::vector<HexFile>& hexFiles, std::ostream& err)
        {
            if (!WriteFile(elfPath, elf))
            {
                err << "Cannot write " << elfPath << ".\n";
                return false;
            }

            for (const auto& [path, lines] : hexFiles)
                if (!WriteLines(path, lines))
                {
                    err << "Cannot write " << path << ".\n";
                    return false;
                }
            return true;
        }

        int InjectKeys(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err)
        {
            const auto keysPath = Option(arguments, "--keys");
            const auto elfPath = Option(arguments, "--elf");
            if (!keysPath)
                return Missing(err, arguments[0], "--keys <key set>");
            if (!elfPath)
                return Missing(err, arguments[0], "--elf <bootloader elf>");
            const auto section = Option(arguments, "--section").value_or(std::string{ keySlotSection });

            const auto slot = LoadKeySlot(*keysPath, err);
            if (!slot)
                return failure;

            auto elf = ReadFile(*elfPath);
            if (!elf)
            {
                err << "Cannot read the ELF file " << *elfPath << ".\n";
                return failure;
            }

            const auto location = LocateKeySlot(*elf, *elfPath, section, err);
            if (!location)
                return failure;

            std::ranges::copy(*slot, elf->begin() + static_cast<std::ptrdiff_t>(location->fileOffset));
            const auto hexFiles = PatchedHexFiles(Options(arguments, "--hex"), location->loadAddress, *slot, err);
            if (!hexFiles || !WriteAllFiles(*elfPath, *elf, *hexFiles, err))
                return failure;

            out << *elfPath << ": key slot from " << *keysPath << " written into " << section << ", and into " << hexFiles->size() << " Intel HEX file(s).\n";
            return success;
        }
    }

    int Run(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random)
    {
        if (arguments.empty())
        {
            err << usage;
            return failure;
        }

        const auto& command = arguments[0];
        if (command == "--help" || command == "-h" || command == "help")
        {
            out << usage;
            return success;
        }
        if (command == "generate-keys")
            return GenerateKeys(arguments, out, err, random);
        if (command == "bootloader-keys")
            return BootloaderKeys(arguments, out, err);
        if (command == "descriptor")
            return Descriptor(arguments, out, err);
        if (command == "combine")
            return CombineImages(arguments, out, err);
        if (command == "inject-keys")
            return InjectKeys(arguments, out, err);

        err << "Unknown command '" << command << "'.\n\n"
            << usage;
        return failure;
    }
}
