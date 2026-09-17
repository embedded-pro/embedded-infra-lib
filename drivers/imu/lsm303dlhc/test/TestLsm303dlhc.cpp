#include "drivers/imu/lsm303dlhc/Lsm303dlhc.hpp"
#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometerWithFifo.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Device = drivers::Lsm303dlhc<>;
    using InitializationResult = Device::InitializationResult;

    class Lsm303dlhcTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectAccelerometerInitialization(uint8_t control1ReadBack = 0x57)
        {
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));
            EXPECT_CALL(accelerometerBus, ReadRegisterMock(0x20, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ control1ReadBack }));

            if (control1ReadBack != 0x57)
                return;

            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x26, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        }

        void ExpectMagnetometerInitialization(std::vector<uint8_t> identification = { 0x48, 0x34, 0x33 })
        {
            EXPECT_CALL(magnetometerBus, ReadRegisterMock(0x0a, 3)).WillOnce(testing::Return(identification));

            if (identification != std::vector<uint8_t>{ 0x48, 0x34, 0x33 })
                return;

            EXPECT_CALL(magnetometerBus, WriteRegisterMock(0x00, std::vector<uint8_t>{ 0x90 }));
            EXPECT_CALL(magnetometerBus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x20 }));
            EXPECT_CALL(magnetometerBus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x00 }));
        }

        void Initialize()
        {
            device.Initialize(Device::Config(), [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(13));
        }

        testing::StrictMock<services::RegisterBusAccessMock> accelerometerBus;
        testing::StrictMock<services::RegisterBusAccessMock> magnetometerBus;
        hal::GpioPinStub accelerometerDataReadyPin;
        hal::GpioPinStub magnetometerDataReadyPin;
        Device device{ accelerometerBus, magnetometerBus, accelerometerDataReadyPin, magnetometerDataReadyPin };
        std::optional<InitializationResult> initializationResult;
    };
}

TEST_F(Lsm303dlhcTest, initialize_configures_both_halves_and_reports_success)
{
    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization();

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Lsm303dlhcTest, initialize_reports_the_accelerometer_when_it_is_missing)
{
    ExpectAccelerometerInitialization(0x00);

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::accelerometerNotFound, *initializationResult);
}

TEST_F(Lsm303dlhcTest, initialize_reports_the_magnetometer_when_it_is_missing)
{
    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization({ 0x00, 0x00, 0x00 });

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::magnetometerNotFound, *initializationResult);
}

TEST_F(Lsm303dlhcTest, as_accelerometer_and_as_magnetometer_stream_independently)
{
    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization();
    Initialize();

    std::vector<int32_t> acceleration;
    std::vector<int32_t> magneticFluxDensity;

    EXPECT_CALL(accelerometerBus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x10 }));

    device.AsAccelerometer().Start([&acceleration](Device::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                acceleration.push_back(sample.Value());
        });
    device.AsMagnetometer().Start([&magneticFluxDensity](Device::Magnetometer::Samples samples)
        {
            for (auto sample : samples)
                magneticFluxDensity.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(accelerometerBus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x80, 0x3e, 0x00, 0x00, 0x00, 0x00 }));
    accelerometerDataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_CALL(magnetometerBus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x04, 0x4c, 0x00, 0x00, 0x00, 0x00 }));
    magnetometerDataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
    ASSERT_EQ(3u, magneticFluxDensity.size());
    EXPECT_EQ(1000, magneticFluxDensity[0]);
}

TEST_F(Lsm303dlhcTest, measure_temperature_is_served_by_the_magnetometer)
{
    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization();
    Initialize();

    EXPECT_CALL(magnetometerBus, ReadRegisterMock(0x31, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0c, 0x80 }));

    std::optional<int32_t> temperature;
    device.MeasureTemperature([&temperature](Device::Temperature value)
        {
            temperature = value.Value();
        });

    ExecuteAllActions();

    ASSERT_TRUE(temperature);
    EXPECT_EQ(25000, *temperature);
}

TEST_F(Lsm303dlhcTest, stop_stops_both_halves_and_reports_done)
{
    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization();
    Initialize();

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcTest, the_cores_are_reachable_for_mixin_specific_configuration)
{
    using FifoDevice = drivers::Lsm303dlhc<drivers::Lsm303dlhcAccelerometerWithFifo<drivers::Lsm303dlhcAccelerometer>>;

    FifoDevice fifoDevice{ accelerometerBus, magnetometerBus };

    ExpectAccelerometerInitialization();
    ExpectMagnetometerInitialization();

    std::optional<FifoDevice::InitializationResult> result;
    fifoDevice.Initialize(FifoDevice::Config(), [&result](FifoDevice::InitializationResult value)
        {
            result = value;
        });

    ForwardTime(std::chrono::milliseconds(13));

    ASSERT_TRUE(result);
    EXPECT_EQ(FifoDevice::InitializationResult::success, *result);

    EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(accelerometerBus, ReadRegisterMock(0x24, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x90 }));
    EXPECT_CALL(accelerometerBus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(accelerometerBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x06 }));

    infra::VerifyingFunction<void()> fifoEnabled;
    fifoDevice.AccelerometerDevice().EnableFifo(drivers::Lsm303dlhcAccelerometerWithFifo<drivers::Lsm303dlhcAccelerometer>::FifoConfig(), fifoEnabled);

    ExecuteAllActions();

    infra::VerifyingFunction<void()> stopped;
    fifoDevice.Stop(stopped);
    ExecuteAllActions();
}
