#include "hal/interfaces/test_doubles/BlockDeviceMock.hpp"
#include "hal/interfaces/test_doubles/BlockDeviceStub.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <limits>

namespace
{
    class BlockDeviceTest
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        static constexpr uint32_t blockSize = 64;
        static constexpr uint32_t numberOfBlocks = 4;

        hal::BlockDeviceStub::WithStorage<blockSize * numberOfBlocks> device{ blockSize };
        std::array<uint8_t, blockSize> secondReadBuffer{};
        bool secondReadDone = false;
    };
}

TEST_F(BlockDeviceTest, BlockSize_ReportsConfiguredBlockSize)
{
    EXPECT_EQ(blockSize, device.BlockSize());
}

TEST_F(BlockDeviceTest, NumberOfBlocks_ReportsConfiguredNumberOfBlocks)
{
    EXPECT_EQ(numberOfBlocks, device.NumberOfBlocks());
}

TEST_F(BlockDeviceTest, WriteBlocks_CompletionNotInvokedBeforeExecuteAllActions)
{
    std::array<uint8_t, blockSize> data{};
    bool done = false;

    device.WriteBlocks(infra::MakeRange(data), 0, [&done](hal::BlockDevice::Result result)
        {
            done = true;
        });

    EXPECT_FALSE(done);
}

TEST_F(BlockDeviceTest, WriteBlocks_CompletionInvokedAfterExecuteAllActions)
{
    std::array<uint8_t, blockSize> data{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::failed;

    device.WriteBlocks(infra::MakeRange(data), 0, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::success, receivedResult);
}

TEST_F(BlockDeviceTest, WriteBlocks_ThenReadBlocks_ReturnsWrittenData)
{
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> readData{};

    for (uint8_t i = 0; i < blockSize; ++i)
        writeData[i] = i;

    device.WriteBlocks(infra::MakeConstRange(writeData), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.ReadBlocks(infra::MakeRange(readData), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(writeData, readData);
}

TEST_F(BlockDeviceTest, WriteBlocks_MultipleBlocks_ThenReadBlocks_ReturnsWrittenData)
{
    std::array<uint8_t, blockSize * 2> writeData{};
    std::array<uint8_t, blockSize * 2> readData{};

    for (std::size_t i = 0; i < writeData.size(); ++i)
        writeData[i] = static_cast<uint8_t>(i);

    device.WriteBlocks(infra::MakeConstRange(writeData), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.ReadBlocks(infra::MakeRange(readData), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(writeData, readData);
}

TEST_F(BlockDeviceTest, WriteBlocks_AtNonZeroFirstBlock_DoesNotDisturbNeighbours)
{
    std::array<uint8_t, blockSize> before{};
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> after{};
    std::array<uint8_t, blockSize> beforeRead{};
    std::array<uint8_t, blockSize> afterRead{};

    for (uint8_t i = 0; i < blockSize; ++i)
    {
        before[i] = i;
        writeData[i] = static_cast<uint8_t>(i + 100);
        after[i] = static_cast<uint8_t>(i + 200);
    }

    device.WriteBlocks(infra::MakeConstRange(before), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.WriteBlocks(infra::MakeConstRange(writeData), 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.WriteBlocks(infra::MakeConstRange(after), 2, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.ReadBlocks(infra::MakeRange(beforeRead), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.ReadBlocks(infra::MakeRange(afterRead), 2, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(before, beforeRead);
    EXPECT_EQ(after, afterRead);
}

TEST_F(BlockDeviceTest, EraseBlocks_CompletesWithSuccess_AffectsOnlyRequestedRange)
{
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> beforeErase{};
    std::array<uint8_t, blockSize> afterErase{};
    std::array<uint8_t, blockSize> beforeRead{};
    std::array<uint8_t, blockSize> afterRead{};

    for (uint8_t i = 0; i < blockSize; ++i)
    {
        writeData[i] = i;
        beforeErase[i] = i;
        afterErase[i] = i;
    }

    device.WriteBlocks(infra::MakeConstRange(beforeErase), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.WriteBlocks(infra::MakeConstRange(writeData), 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.WriteBlocks(infra::MakeConstRange(afterErase), 2, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    hal::BlockDevice::Result eraseResult = hal::BlockDevice::Result::failed;
    device.EraseBlocks(1, 2, [&eraseResult](hal::BlockDevice::Result result)
        {
            eraseResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::success, eraseResult);

    std::array<uint8_t, blockSize> erasedRead{};
    device.ReadBlocks(infra::MakeRange(erasedRead), 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.ReadBlocks(infra::MakeRange(beforeRead), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.ReadBlocks(infra::MakeRange(afterRead), 2, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    std::array<uint8_t, blockSize> allZeros{};
    EXPECT_EQ(allZeros, erasedRead);
    EXPECT_EQ(beforeErase, beforeRead);
    EXPECT_EQ(afterErase, afterRead);
}

TEST_F(BlockDeviceTest, ReadBlocks_FirstBlockEqualsNumberOfBlocks_CompletesWithOutOfRange)
{
    std::array<uint8_t, blockSize> readData{};
    std::array<uint8_t, blockSize * numberOfBlocks> initialStorage{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    for (std::size_t i = 0; i < initialStorage.size(); ++i)
        initialStorage[i] = static_cast<uint8_t>(i);

    device.WriteBlocks(infra::MakeConstRange(initialStorage), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.ReadBlocks(infra::MakeRange(readData), numberOfBlocks, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, ReadBlocks_RangeCrossesEnd_CompletesWithOutOfRange)
{
    std::array<uint8_t, blockSize * 2> readData{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    device.ReadBlocks(infra::MakeRange(readData), numberOfBlocks - 1, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, ReadBlocks_Uint32Overflow_CompletesWithOutOfRange)
{
    std::array<uint8_t, blockSize> readData{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    device.ReadBlocks(infra::MakeRange(readData), std::numeric_limits<uint32_t>::max(), [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, WriteBlocks_OutOfRange_CompletesWithOutOfRangeAndStorageUnchanged)
{
    std::array<uint8_t, blockSize> initial{};
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> readBack{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    for (uint8_t i = 0; i < blockSize; ++i)
    {
        initial[i] = i;
        writeData[i] = static_cast<uint8_t>(i + 100);
    }

    device.WriteBlocks(infra::MakeConstRange(initial), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.WriteBlocks(infra::MakeConstRange(writeData), numberOfBlocks, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);

    device.ReadBlocks(infra::MakeRange(readBack), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(initial, readBack);
}

TEST_F(BlockDeviceTest, WriteBlocks_RangeCrossesEnd_CompletesWithOutOfRangeAndStorageUnchanged)
{
    std::array<uint8_t, blockSize> initial{};
    std::array<uint8_t, blockSize * 2> writeData{};
    std::array<uint8_t, blockSize> readBack{};
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    for (uint8_t i = 0; i < blockSize; ++i)
        initial[i] = i;

    device.WriteBlocks(infra::MakeConstRange(initial), numberOfBlocks - 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.WriteBlocks(infra::MakeConstRange(writeData), numberOfBlocks - 1, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);

    device.ReadBlocks(infra::MakeRange(readBack), numberOfBlocks - 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(initial, readBack);
}

TEST_F(BlockDeviceTest, EraseBlocks_BeginGreaterThanEnd_CompletesWithOutOfRange)
{
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    device.EraseBlocks(2, 1, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, EraseBlocks_RangeCrossesEnd_CompletesWithOutOfRange)
{
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    device.EraseBlocks(numberOfBlocks - 1, numberOfBlocks + 1, [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, EraseBlocks_Uint32Overflow_CompletesWithOutOfRange)
{
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::success;

    device.EraseBlocks(1, std::numeric_limits<uint32_t>::max(), [&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::outOfRange, receivedResult);
}

TEST_F(BlockDeviceTest, FailNextOperationWith_DeliversChosenResultExactlyOnce)
{
    std::array<uint8_t, blockSize> data{};
    hal::BlockDevice::Result firstResult = hal::BlockDevice::Result::success;
    hal::BlockDevice::Result secondResult = hal::BlockDevice::Result::failed;

    device.FailNextOperationWith(hal::BlockDevice::Result::timeout);

    device.WriteBlocks(infra::MakeConstRange(data), 0, [&firstResult](hal::BlockDevice::Result result)
        {
            firstResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::timeout, firstResult);

    device.WriteBlocks(infra::MakeConstRange(data), 0, [&secondResult](hal::BlockDevice::Result result)
        {
            secondResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::success, secondResult);
}

TEST_F(BlockDeviceTest, ReadBlocks_InCompletionCallback_StartsNewOperation_Works)
{
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> firstReadBuffer{};

    for (uint8_t i = 0; i < blockSize; ++i)
        writeData[i] = i;

    device.WriteBlocks(infra::MakeConstRange(writeData), 0, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    device.ReadBlocks(infra::MakeRange(firstReadBuffer), 0, [this](hal::BlockDevice::Result)
        {
            device.ReadBlocks(infra::MakeRange(secondReadBuffer), 0, [this](hal::BlockDevice::Result)
                {
                    secondReadDone = true;
                });
        });
    ExecuteAllActions();

    EXPECT_EQ(writeData, firstReadBuffer);
    EXPECT_TRUE(secondReadDone);
    EXPECT_EQ(writeData, secondReadBuffer);
}

TEST_F(BlockDeviceTest, Flush_CompletionNotInvokedBeforeExecuteAllActions)
{
    bool done = false;

    device.Flush([&done](hal::BlockDevice::Result)
        {
            done = true;
        });

    EXPECT_FALSE(done);
}

TEST_F(BlockDeviceTest, Flush_CompletesWithSuccess)
{
    hal::BlockDevice::Result receivedResult = hal::BlockDevice::Result::failed;

    device.Flush([&receivedResult](hal::BlockDevice::Result result)
        {
            receivedResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::success, receivedResult);
}

TEST_F(BlockDeviceTest, Flush_AfterFailNextOperationWith_DeliversChosenResultOnce)
{
    hal::BlockDevice::Result firstResult = hal::BlockDevice::Result::success;
    hal::BlockDevice::Result secondResult = hal::BlockDevice::Result::failed;

    device.FailNextOperationWith(hal::BlockDevice::Result::timeout);

    device.Flush([&firstResult](hal::BlockDevice::Result result)
        {
            firstResult = result;
        });
    ExecuteAllActions();
    device.Flush([&secondResult](hal::BlockDevice::Result result)
        {
            secondResult = result;
        });
    ExecuteAllActions();

    EXPECT_EQ(hal::BlockDevice::Result::timeout, firstResult);
    EXPECT_EQ(hal::BlockDevice::Result::success, secondResult);
}

TEST_F(BlockDeviceTest, Flush_DoesNotChangeStorage)
{
    std::array<uint8_t, blockSize> writeData{};
    std::array<uint8_t, blockSize> readData{};

    for (uint8_t i = 0; i < blockSize; ++i)
        writeData[i] = i;

    device.WriteBlocks(infra::MakeConstRange(writeData), 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.Flush([](hal::BlockDevice::Result) {});
    ExecuteAllActions();
    device.ReadBlocks(infra::MakeRange(readData), 1, [](hal::BlockDevice::Result) {});
    ExecuteAllActions();

    EXPECT_EQ(writeData, readData);
}

TEST(BlockDeviceMockTest, Mock_OnDoneCallback_CanBeInvokedWithEachResult)
{
    using Result = hal::BlockDevice::Result;

    testing::StrictMock<hal::BlockDeviceMock> mock;
    infra::Function<void(Result)> capturedCallback;

    std::array<uint8_t, 64> buffer{};

    EXPECT_CALL(mock, ReadBlocks(testing::_, 0, testing::_))
        .WillOnce([&capturedCallback](infra::ByteRange, uint32_t, const infra::Function<void(Result)>& onDone)
            {
                capturedCallback = onDone;
            });

    mock.ReadBlocks(infra::MakeRange(buffer), 0, [](Result) {});

    for (auto result : { Result::success, Result::notPresent, Result::timeout, Result::crcError,
             Result::writeProtected, Result::outOfRange, Result::failed })
    {
        capturedCallback(result);
    }
}

TEST(BlockDeviceMockTest, Mock_Flush_CanBeExpected)
{
    using Result = hal::BlockDevice::Result;

    testing::StrictMock<hal::BlockDeviceMock> mock;
    Result receivedResult = Result::failed;

    EXPECT_CALL(mock, Flush(testing::_))
        .WillOnce([](const infra::Function<void(Result)>& onDone)
            {
                onDone(Result::success);
            });

    mock.Flush([&receivedResult](Result result)
        {
            receivedResult = result;
        });

    EXPECT_EQ(Result::success, receivedResult);
}
