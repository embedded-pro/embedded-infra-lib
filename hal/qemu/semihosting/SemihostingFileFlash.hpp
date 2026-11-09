#ifndef HAL_QEMU_SEMIHOSTING_FILE_FLASH_HPP
#define HAL_QEMU_SEMIHOSTING_FILE_FLASH_HPP

#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include <cstdint>

namespace hal
{
    class SemihostingFileFlash
        : public services::SynchronousFlashMemory
    {
    public:
        SemihostingFileFlash(SemihostingFileSystem& fileSystem, const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override;

    private:
        bool Load();
        void StoreAll() const;
        void Store(uint32_t address, uint32_t length) const;

        SemihostingFileSystem& fileSystem;
        const char* path;
        infra::ByteRange storage;
    };
}

#endif
