#ifndef APPLICATION_BOOTLOADER_IMAGE_PACK_HPP
#define APPLICATION_BOOTLOADER_IMAGE_PACK_HPP

#include "application/bootloader_image/KeySet.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace application
{
    struct PackDescription
    {
        std::string productName;
        std::string productVersion;
        std::string componentName;
        uint32_t destination;
        std::vector<uint8_t> image;
    };

    enum class PackError
    {
        nameTooLong,
        signingFailed,
    };

    using InitialCounter = std::array<uint8_t, 16>;

    std::variant<std::vector<uint8_t>, PackError> BuildPack(const PackDescription& pack, const KeySet& keys, const InitialCounter& counter);
}

#endif
