#ifndef APPLICATION_BOOTLOADER_IMAGE_TEST_HEX_RECORD_HPP
#define APPLICATION_BOOTLOADER_IMAGE_TEST_HEX_RECORD_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace application
{
    inline std::string HexRecord(uint16_t address, uint8_t type, const std::vector<uint8_t>& data)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::vector<uint8_t> bytes{ static_cast<uint8_t>(data.size()), static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address), type };
        bytes.insert(bytes.end(), data.begin(), data.end());
        uint8_t sum = 0;
        for (auto byte : bytes)
            sum = static_cast<uint8_t>(sum + byte);
        bytes.push_back(static_cast<uint8_t>(-sum));

        std::string record{ ":" };
        for (auto byte : bytes)
            record += { digits[byte >> 4], digits[byte & 0xf] };
        return record;
    }
}

#endif
