#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "drivers/imu/mpu9250/test/Mpu9250BusAccessMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using InitializationResult = drivers::Mpu9250Core::InitializationResult;

    class Mpu9250LifetimeTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Mpu9250LifetimeTest()
        {
            EXPECT_CALL(bus, RequiresI2cSlaveInterfaceDisabled()).WillRepeatedly(testing::Return(false));
        }

        void Initialize(drivers::Mpu9250Core& device)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x75, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x71 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x04 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1a, std::vector<uint8_t>{ 0x03 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x03 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x37, std::vector<uint8_t>{ 0x10 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(drivers::Mpu9250Core::Config(), [](InitializationResult) {});

            ForwardTime(std::chrono::milliseconds(101));
        }

        void StartStreaming(drivers::Mpu9250Core& device)
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

            device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
                {
                    bursts += 1;
                });

            ExecuteAllActions();
        }

        testing::StrictMock<drivers::Mpu9250BusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        int bursts = 0;
    };
}

TEST_F(Mpu9250LifetimeTest, destruction_disarms_the_data_ready_interrupt)
{
    {
        drivers::Mpu9250Core device{ bus, dataReadyPin };
        Initialize(device);
        StartStreaming(device);
    }

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, bursts);
}

TEST_F(Mpu9250LifetimeTest, stop_disarms_the_interrupt_and_delivers_no_further_samples)
{
    drivers::Mpu9250Core device{ bus, dataReadyPin };
    Initialize(device);
    StartStreaming(device);

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);
    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, bursts);
}

TEST_F(Mpu9250LifetimeTest, stop_when_idle_still_reports_done)
{
    drivers::Mpu9250Core device{ bus, dataReadyPin };
    Initialize(device);

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    ExecuteAllActions();

    EXPECT_TRUE(stopped);
}

TEST_F(Mpu9250LifetimeTest, stop_defers_until_an_outstanding_transaction_completes)
{
    drivers::Mpu9250Core device{ bus, dataReadyPin };
    Initialize(device);

    EXPECT_CALL(bus, ReadRegisterMock(0x41, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x00 }));

    device.MeasureTemperature([](drivers::Mpu9250Core::Temperature) {});

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    EXPECT_FALSE(stopped);

    ExecuteAllActions();

    EXPECT_TRUE(stopped);
}

TEST_F(Mpu9250LifetimeTest, stop_aborts_a_running_sequence_without_invoking_its_callback)
{
    drivers::Mpu9250Core device{ bus, dataReadyPin };
    Initialize(device);

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));

    bool powerModeSet = false;
    device.SetPowerMode(drivers::Mpu9250Core::PowerMode::sleep, [&powerModeSet]()
        {
            powerModeSet = true;
        });

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    ExecuteAllActions();

    EXPECT_FALSE(powerModeSet);
    EXPECT_TRUE(stopped);
}

TEST_F(Mpu9250LifetimeTest, a_device_without_a_data_ready_pin_stops_cleanly)
{
    drivers::Mpu9250Core device{ bus };
    Initialize(device);

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);

    ExecuteAllActions();
}
