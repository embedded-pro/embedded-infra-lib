#ifndef APPLICATION_BOOTLOADER_IMAGE_ELF_SECTION_HPP
#define APPLICATION_BOOTLOADER_IMAGE_ELF_SECTION_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace application
{
    struct ElfSection
    {
        std::size_t fileOffset;
        std::size_t size;
        uint32_t loadAddress;
    };

    enum class ElfError
    {
        notElf,
        malformed,
        sectionNotFound,
        sectionNotLoaded,
    };

    std::variant<ElfSection, ElfError> FindElfSection(std::span<const uint8_t> elf, std::string_view name);
}

#endif
