#ifndef HAL_QEMU_SEMIHOSTING_ASYNC_FLASH_HPP
#define HAL_QEMU_SEMIHOSTING_ASYNC_FLASH_HPP

#include "hal/interfaces/FlashHomogeneous.hpp"
#include "hal/qemu/semihosting/SemihostingFileFlash.hpp"
#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    class SemihostingAsyncFlash
        : public FlashHomogeneous
    {
    public:
        SemihostingAsyncFlash(SemihostingFileSystem& fileSystem, const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone) override;

    private:
        SemihostingFileFlash file;
    };
}

#endif
