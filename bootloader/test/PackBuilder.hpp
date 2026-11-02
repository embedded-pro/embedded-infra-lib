#ifndef BOOTLOADER_TEST_PACK_BUILDER_HPP
#define BOOTLOADER_TEST_PACK_BUILDER_HPP

#include "application/bootloader_image/Pack.hpp"
#include "uECC.h"
#include <array>
#include <gtest/gtest.h>
#include <string>
#include <variant>
#include <vector>

namespace bootloader
{
    inline constexpr std::array<uint8_t, 16> testAesKey{ 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f };
    inline constexpr const char* testProductName = "test board";

    struct TestKeyPair
    {
        TestKeyPair()
        {
            EXPECT_EQ(1, uECC_make_key(publicKey.data(), privateKey.data(), uECC_secp256r1()));
        }

        std::array<uint8_t, 64> publicKey{};
        std::array<uint8_t, 32> privateKey{};
    };

    struct PackDescription
    {
        std::vector<uint8_t> image;
        std::string productName{ testProductName };
        uint32_t destination{ 0x00008000 };
        std::array<uint8_t, 16> initialCounter{ 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf };
    };

    inline std::vector<uint8_t> BuildPack(const PackDescription& pack, const TestKeyPair& keys)
    {
        const application::KeySet keySet{ testAesKey, keys.publicKey, keys.privateKey };
        const auto built = application::BuildPack({ pack.productName, "v1.7.0", "sync_foc_sensored", pack.destination, pack.image }, keySet, pack.initialCounter);
        EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(built));
        return std::holds_alternative<std::vector<uint8_t>>(built) ? std::get<std::vector<uint8_t>>(built) : std::vector<uint8_t>{};
    }
}

#endif
