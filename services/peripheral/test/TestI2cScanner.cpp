#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/peripheral/I2cScanner.hpp"
#include "gtest/gtest.h"

namespace
{
    constexpr uint16_t firstAddress = 0x08;
    constexpr uint16_t lastAddress = 0x77;
}

class I2cScannerTest
    : public testing::Test
{
public:
    void StartScan()
    {
        scanner.Scan([this](hal::I2cAddress address)
            {
                deviceFound.callback(address);
            },
            [this](uint32_t numberOfDevices)
            {
                done.callback(numberOfDevices);
            });
    }

    void ExpectProbe(uint16_t address)
    {
        EXPECT_CALL(i2c, ReceiveDataMock(hal::I2cAddress(address), hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0 }));
    }

    void ExpectProbes(uint16_t first, uint16_t last)
    {
        for (uint16_t address = first; address <= last; ++address)
            ExpectProbe(address);
    }

    void Answer(hal::Result result)
    {
        auto onReceived = i2c.onReceived;
        onReceived(result);
    }

    void AnswerAll(hal::Result result)
    {
        for (uint16_t address = firstAddress; address <= lastAddress; ++address)
            Answer(result);
    }

    testing::StrictMock<hal::I2cMasterMockWithoutAutomaticDone> i2c;
    testing::StrictMock<infra::MockCallback<void(hal::I2cAddress)>> deviceFound;
    testing::StrictMock<infra::MockCallback<void(uint32_t)>> done;
    services::I2cScanner scanner{ i2c };
};

TEST_F(I2cScannerTest, ScanStartsWithAReceiveOfOneByteAndAStopAtTheFirstAddress)
{
    ExpectProbe(firstAddress);

    StartScan();
}

TEST_F(I2cScannerTest, EveryAddressIsProbedInOrderAndTheScanEndsWithoutDevices)
{
    testing::InSequence sequence;
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(done, callback(0));

    StartScan();
    AnswerAll(hal::Result::partialComplete);
}

TEST_F(I2cScannerTest, ADeviceThatAcknowledgesIsReportedAndCounted)
{
    testing::InSequence sequence;
    ExpectProbes(firstAddress, 0x19);
    ExpectProbe(0x1a);
    EXPECT_CALL(deviceFound, callback(hal::I2cAddress(0x1a)));
    ExpectProbes(0x1b, 0x37);
    ExpectProbe(0x38);
    EXPECT_CALL(deviceFound, callback(hal::I2cAddress(0x38)));
    ExpectProbes(0x39, lastAddress);
    EXPECT_CALL(done, callback(2));

    StartScan();
    for (uint16_t address = firstAddress; address <= lastAddress; ++address)
        Answer(address == 0x1a || address == 0x38 ? hal::Result::complete : hal::Result::partialComplete);
}

TEST_F(I2cScannerTest, ABusErrorIsNotReportedAsADevice)
{
    testing::InSequence sequence;
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(done, callback(0));

    StartScan();
    AnswerAll(hal::Result::busError);
}

TEST_F(I2cScannerTest, ScanningLastsUntilTheLastAddressIsAnswered)
{
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(done, callback(0));
    EXPECT_FALSE(scanner.Scanning());

    StartScan();
    EXPECT_TRUE(scanner.Scanning());

    for (uint16_t address = firstAddress; address != lastAddress; ++address)
        Answer(hal::Result::partialComplete);
    EXPECT_TRUE(scanner.Scanning());

    Answer(hal::Result::partialComplete);
    EXPECT_FALSE(scanner.Scanning());
}

TEST_F(I2cScannerTest, TheCountStartsAgainWithEveryScan)
{
    testing::InSequence sequence;
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(deviceFound, callback(hal::I2cAddress(lastAddress)));
    EXPECT_CALL(done, callback(1));
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(done, callback(0));

    StartScan();
    for (uint16_t address = firstAddress; address != lastAddress; ++address)
        Answer(hal::Result::partialComplete);
    Answer(hal::Result::complete);

    StartScan();
    AnswerAll(hal::Result::partialComplete);
}

TEST_F(I2cScannerTest, ANewScanCanBeStartedFromTheDoneCallback)
{
    testing::InSequence sequence;
    ExpectProbes(firstAddress, lastAddress);
    EXPECT_CALL(done, callback(0)).WillOnce(testing::Invoke([this](uint32_t)
        {
            EXPECT_FALSE(scanner.Scanning());
            StartScan();
        }));
    ExpectProbe(firstAddress);

    StartScan();
    AnswerAll(hal::Result::partialComplete);

    EXPECT_TRUE(scanner.Scanning());
}
