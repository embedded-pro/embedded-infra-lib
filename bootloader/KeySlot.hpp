#ifndef BOOTLOADER_KEY_SLOT_HPP
#define BOOTLOADER_KEY_SLOT_HPP

#include <array>
#include <cstdint>

namespace bootloader
{
    struct KeySlot
    {
        std::array<uint8_t, 64> signaturePublicKey;
        std::array<uint8_t, 16> imageEncryptionKey;
    };

    static_assert(sizeof(KeySlot) == 80);

    constexpr KeySlot ErasedKeySlot()
    {
        KeySlot slot{};
        slot.signaturePublicKey.fill(0xff);
        slot.imageEncryptionKey.fill(0xff);
        return slot;
    }
}

#endif
