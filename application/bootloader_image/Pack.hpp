#ifndef APPLICATION_BOOTLOADER_IMAGE_PACK_HPP
#define APPLICATION_BOOTLOADER_IMAGE_PACK_HPP

#include "application/bootloader_image/KeySet.hpp"
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
        randomSourceFailed,
        signingFailed,
    };

    std::variant<std::vector<uint8_t>, PackError> BuildPack(const PackDescription& pack, const KeySet& keys, const RandomSource& random);
}

#endif
