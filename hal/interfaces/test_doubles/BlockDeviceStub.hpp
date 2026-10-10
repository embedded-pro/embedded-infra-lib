#ifndef HAL_BLOCK_DEVICE_STUB_HPP
#define HAL_BLOCK_DEVICE_STUB_HPP

#include "hal/interfaces/BlockDevice.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace hal
{
    class BlockDeviceStub
        : public BlockDevice
    {
    public:
        BlockDeviceStub(infra::ByteRange storage, uint32_t blockSize)
            : storage(storage)
            , blockSize(blockSize)
        {}

        uint32_t BlockSize() const override;
        uint32_t NumberOfBlocks() const override;
        void ReadBlocks(infra::ByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone) override;
        void WriteBlocks(infra::ConstByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDone) override;
        void EraseBlocks(uint32_t beginBlock, uint32_t endBlock, const infra::Function<void(Result)>& onDone) override;
        void Flush(const infra::Function<void(Result)>& onDone) override;

        void FailNextOperationWith(Result result);

        template<std::size_t StorageSize>
        using WithStorage = infra::WithStorage<BlockDeviceStub, std::array<uint8_t, StorageSize>>;

    private:
        bool IsRangeValid(uint32_t firstBlock, uint32_t blockCount) const;
        std::optional<Result> ConsumeInjectedFailure();
        void ScheduleCompletion(Result result);

    private:
        infra::ByteRange storage;
        uint32_t blockSize;
        infra::AutoResetFunction<void(Result)> onDone;
        std::optional<Result> injectedFailure;
    };
}

#endif
