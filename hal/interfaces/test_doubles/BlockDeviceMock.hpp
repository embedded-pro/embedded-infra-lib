#ifndef HAL_BLOCK_DEVICE_MOCK_HPP
#define HAL_BLOCK_DEVICE_MOCK_HPP

#include "hal/interfaces/BlockDevice.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class BlockDeviceMock
        : public BlockDevice
    {
    public:
        MOCK_METHOD(uint32_t, BlockSize, (), (const, override));
        MOCK_METHOD(uint32_t, NumberOfBlocks, (), (const, override));
        MOCK_METHOD(void, ReadBlocks, (infra::ByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(void, WriteBlocks, (infra::ConstByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(void, EraseBlocks, (uint32_t beginBlock, uint32_t endBlock, const infra::Function<void(Result)>& onDone), (override));
    };
}

#endif
