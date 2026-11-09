#ifndef HAL_QEMU_SEMIHOSTING_EEPROM_HPP
#define HAL_QEMU_SEMIHOSTING_EEPROM_HPP

#include "hal/interfaces/Eeprom.hpp"
#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace hal
{
    class SemihostingEeprom
        : public Eeprom
    {
    public:
        template<std::size_t StorageSize>
        using WithStorage = infra::WithStorage<SemihostingEeprom, std::array<uint8_t, StorageSize>>;

        SemihostingEeprom(infra::ByteRange storage, SemihostingFileSystem& fileSystem, const char* path);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        void LoadFromFile();
        void FlushToFile() const;
        void Complete(const infra::Function<void()>& onDone) const;

        infra::ByteRange storage;
        SemihostingFileSystem& fileSystem;
        const char* path;
    };
}

#endif
