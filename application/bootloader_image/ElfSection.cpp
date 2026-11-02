#include "application/bootloader_image/ElfSection.hpp"
#include "upgrade/pack_builder/Elf.hpp"
#include <cstring>
#include <optional>

namespace application
{
    namespace
    {
        constexpr uint32_t elfMagic = 0x464c457f;
        constexpr uint8_t elfClass32 = 1;
        constexpr uint8_t littleEndian = 1;
        constexpr uint32_t sectionNoBits = 8;
        constexpr uint32_t segmentLoad = 1;

        bool Contains(std::span<const uint8_t> elf, uint64_t offset, uint64_t size)
        {
            return offset <= elf.size() && size <= elf.size() - offset;
        }

        template<class T>
        std::optional<T> ReadAt(std::span<const uint8_t> elf, uint64_t offset)
        {
            if (!Contains(elf, offset, sizeof(T)))
                return std::nullopt;

            T value{};
            std::memcpy(&value, elf.data() + offset, sizeof(T));
            return value;
        }

        std::optional<elf_section_header_t> SectionHeader(std::span<const uint8_t> elf, const elf_header_t& header, uint16_t index)
        {
            return ReadAt<elf_section_header_t>(elf, header.section_header_offset + uint64_t{ index } * header.section_header_entry_size);
        }

        std::optional<std::string_view> NameOf(std::span<const uint8_t> elf, const elf_section_header_t& names, uint32_t offset)
        {
            if (!Contains(elf, names.data_offset, names.data_size_in_file) || offset >= names.data_size_in_file)
                return std::nullopt;

            const auto* begin = reinterpret_cast<const char*>(elf.data() + names.data_offset + offset);
            const auto available = names.data_size_in_file - offset;
            const auto* end = static_cast<const char*>(std::memchr(begin, '\0', available));
            if (end == nullptr)
                return std::nullopt;
            return std::string_view{ begin, static_cast<std::size_t>(end - begin) };
        }

        bool IsElf32LittleEndian(const std::optional<elf_header_t>& header)
        {
            return header && header->ident.magic == elfMagic && header->ident.elfclass == elfClass32 && header->ident.data == littleEndian;
        }

        bool HasExpectedTables(const elf_header_t& header)
        {
            return header.section_header_entry_size == sizeof(elf_section_header_t) &&
                   (header.program_header_entry_count == 0 || header.program_header_entry_size == sizeof(elf_program_header_t));
        }

        std::variant<ElfSection, ElfError> Located(std::span<const uint8_t> elf, const elf_header_t& header, const elf_section_header_t& section)
        {
            if (section.type == sectionNoBits)
                return ElfError::sectionNotLoaded;
            if (!Contains(elf, section.data_offset, section.data_size_in_file))
                return ElfError::malformed;

            const uint64_t begin = section.data_offset;
            const uint64_t end = begin + section.data_size_in_file;
            for (uint16_t index = 0; index < header.program_header_entry_count; ++index)
            {
                const auto segment = ReadAt<elf_program_header_t>(elf, header.program_header_offset + uint64_t{ index } * header.program_header_entry_size);
                if (!segment)
                    return ElfError::malformed;
                if (segment->type == segmentLoad && begin >= segment->data_offset && end <= uint64_t{ segment->data_offset } + segment->data_size_in_file)
                    return ElfSection{ section.data_offset, section.data_size_in_file, static_cast<uint32_t>(segment->physical_address + (begin - segment->data_offset)) };
            }

            return ElfError::sectionNotLoaded;
        }
    }

    std::variant<ElfSection, ElfError> FindElfSection(std::span<const uint8_t> elf, std::string_view name)
    {
        const auto header = ReadAt<elf_header_t>(elf, 0);
        if (!IsElf32LittleEndian(header))
            return ElfError::notElf;
        if (!HasExpectedTables(*header))
            return ElfError::malformed;

        const auto names = SectionHeader(elf, *header, header->string_table_index);
        if (!names)
            return ElfError::malformed;

        for (uint16_t index = 0; index < header->section_header_entry_count; ++index)
        {
            const auto section = SectionHeader(elf, *header, index);
            const auto sectionName = section ? NameOf(elf, *names, section->name) : std::nullopt;
            if (!sectionName)
                return ElfError::malformed;
            if (*sectionName == name)
                return Located(elf, *header, *section);
        }

        return ElfError::sectionNotFound;
    }
}
