#ifndef HAL_BLOCK_DEVICE_HPP
#define HAL_BLOCK_DEVICE_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    class BlockDevice
    {
    protected:
        BlockDevice() = default;
        BlockDevice(const BlockDevice& other) = delete;
        BlockDevice& operator=(const BlockDevice& other) = delete;
        ~BlockDevice() = default;

    public:
        enum class Result : uint8_t
        {
            success,
            notPresent,
            timeout,
            crcError,
            writeProtected,
            outOfRange,
            failed
        };

        virtual uint32_t BlockSize() const = 0;
        virtual uint32_t NumberOfBlocks() const = 0;
        virtual void ReadBlocks(infra::ByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone) = 0;
        virtual void WriteBlocks(infra::ConstByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone) = 0;
        // The contents of the blocks are undefined afterwards: implementations may erase, trim or discard them
        virtual void EraseBlocks(uint32_t beginBlock, uint32_t endBlock, const infra::Function<void(Result)>& onDone) = 0;
        // Completes once every write that has completed is durable on the medium, including data held in a cache of the device
        virtual void Flush(const infra::Function<void(Result)>& onDone) = 0;
    };
}

#endif
