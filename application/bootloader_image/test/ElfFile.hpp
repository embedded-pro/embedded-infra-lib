#ifndef APPLICATION_BOOTLOADER_IMAGE_TEST_ELF_FILE_HPP
#define APPLICATION_BOOTLOADER_IMAGE_TEST_ELF_FILE_HPP

#include "upgrade/pack_builder/Elf.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace application
{
    struct ElfFileSection
    {
        std::string name;
        uint32_t address;
        uint32_t loadAddress;
        std::vector<uint8_t> contents;
        uint32_t type{ 1 };
    };

    class ElfFile
    {
    public:
        explicit ElfFile(std::vector<ElfFileSection> sections)
            : sections(std::move(sections))
        {}

        std::vector<uint8_t> Bytes() const
        {
            std::vector<uint8_t> file(sizeof(elf_header_t) + sections.size() * sizeof(elf_program_header_t));
            std::vector<elf_program_header_t> segments;
            std::vector<elf_section_header_t> headers(1);
            std::string names(1, '\0');

            for (const auto& section : sections)
            {
                const auto offset = static_cast<uint32_t>(file.size());
                file.insert(file.end(), section.contents.begin(), section.contents.end());
                const auto size = static_cast<uint32_t>(section.contents.size());
                headers.push_back(elf_section_header_t{ static_cast<uint32_t>(names.size()), section.type, 2, section.address, offset, size, 0, 0, 4, 0 });
                if (section.type == 1)
                    segments.push_back(elf_program_header_t{ 1, offset, section.address, section.loadAddress, size, size, 5, 4 });
                names += section.name + '\0';
            }

            const auto namesOffset = static_cast<uint32_t>(file.size());
            headers.push_back(elf_section_header_t{ static_cast<uint32_t>(names.size()), 3, 0, 0, namesOffset, 0, 0, 0, 1, 0 });
            names += std::string{ ".shstrtab" } + '\0';
            headers.back().data_size_in_file = static_cast<uint32_t>(names.size());
            file.insert(file.end(), names.begin(), names.end());

            const auto headersOffset = static_cast<uint32_t>(file.size());
            file.resize(file.size() + headers.size() * sizeof(elf_section_header_t));
            std::memcpy(file.data() + headersOffset, headers.data(), headers.size() * sizeof(elf_section_header_t));
            std::memcpy(file.data() + sizeof(elf_header_t), segments.data(), segments.size() * sizeof(elf_program_header_t));

            elf_header_t header{ { 0x464c457f, 1, 1, 1, {} }, 2, 40, 1, 0, sizeof(elf_header_t), headersOffset, 0, sizeof(elf_header_t),
                sizeof(elf_program_header_t), static_cast<uint16_t>(segments.size()), sizeof(elf_section_header_t), static_cast<uint16_t>(headers.size()),
                static_cast<uint16_t>(headers.size() - 1) };
            std::memcpy(file.data(), &header, sizeof(header));
            return file;
        }

    private:
        std::vector<ElfFileSection> sections;
    };
}

#endif
