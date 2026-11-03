#ifndef SERVICES_SYNCHRONOUS_FLASH_MEMORY_HPP
#define SERVICES_SYNCHRONOUS_FLASH_MEMORY_HPP

#include "hal/synchronous_interfaces/SynchronousFlashHomogeneous.hpp"
#include "infra/util/ByteRange.hpp"

namespace services
{
    class SynchronousFlashMemory
        : public hal::SynchronousFlashHomogeneous
    {
    public:
        SynchronousFlashMemory(infra::ByteRange memory, uint32_t sizeOfEachSector);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override;

    private:
        infra::ByteRange memory;
    };
}

#endif
