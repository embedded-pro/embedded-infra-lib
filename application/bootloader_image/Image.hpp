#ifndef APPLICATION_BOOTLOADER_IMAGE_IMAGE_HPP
#define APPLICATION_BOOTLOADER_IMAGE_IMAGE_HPP

#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace application
{
    std::vector<uint8_t> ImageDescriptorFor(std::span<const uint8_t> image);

    struct CombineError
    {
        enum class Source
        {
            bootloader,
            application,
        };

        Source source;
        std::size_t line;
    };

    std::variant<std::vector<std::string>, CombineError> Combine(const std::vector<std::string>& bootloader, const std::vector<std::string>& application);

    bool PatchHex(std::vector<std::string>& lines, uint32_t address, std::span<const uint8_t> bytes);
}

#endif
