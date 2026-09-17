#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometer.hpp"
#include "drivers/imu/lsm303dlhc/Lsm303dlhcMagnetometer.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <vector>

namespace
{
    using AccelerometerCore = drivers::Lsm303dlhcAccelerometer;
    using MagnetometerCore = drivers::Lsm303dlhcMagnetometer;

    class Lsm303dlhcLifetimeTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Initialize(AccelerometerCore& device)
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));
            EXPECT_CALL(bus, ReadRegisterMock(0x20, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x57 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x26, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(AccelerometerCore::Config(), [](AccelerometerCore::InitializationResult) {});

            ForwardTime(std::chrono::milliseconds(7));
        }

        void Initialize(MagnetometerCore& device)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x0a, 3)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x48, 0x34, 0x33 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x00, std::vector<uint8_t>{ 0x90 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x20 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(MagnetometerCore::Config(), [](MagnetometerCore::InitializationResult) {});

            ForwardTime(std::chrono::milliseconds(6));
        }

        void ExpectEnableDataReadyInterrupt(uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ result }));
        }

        void StartStreaming(AccelerometerCore& device)
        {
            ExpectEnableDataReadyInterrupt(0x00, 0x10);

            device.AsAccelerometer().Start([this](AccelerometerCore::Accelerometer::Samples)
                {
                    bursts += 1;
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        int bursts = 0;
    };
}

TEST_F(Lsm303dlhcLifetimeTest, destruction_disarms_the_data_ready_interrupt)
{
    {
        AccelerometerCore device{ bus, dataReadyPin };
        Initialize(device);
        StartStreaming(device);
    }

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, bursts);
}

TEST_F(Lsm303dlhcLifetimeTest, stop_disarms_the_interrupt_and_delivers_no_further_samples)
{
    AccelerometerCore device{ bus, dataReadyPin };
    Initialize(device);
    StartStreaming(device);

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);
    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, bursts);
}

TEST_F(Lsm303dlhcLifetimeTest, stop_when_idle_still_reports_done)
{
    AccelerometerCore device{ bus, dataReadyPin };
    Initialize(device);

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    ExecuteAllActions();

    EXPECT_TRUE(stopped);
}

TEST_F(Lsm303dlhcLifetimeTest, stop_defers_until_an_outstanding_transaction_completes)
{
    MagnetometerCore device{ bus, dataReadyPin };
    Initialize(device);

    EXPECT_CALL(bus, ReadRegisterMock(0x31, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x00 }));

    device.MeasureTemperature([](MagnetometerCore::Temperature) {});

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    EXPECT_FALSE(stopped);

    ExecuteAllActions();

    EXPECT_TRUE(stopped);
}

TEST_F(Lsm303dlhcLifetimeTest, stop_aborts_a_running_sequence_without_invoking_its_callback)
{
    AccelerometerCore device{ bus, dataReadyPin };
    Initialize(device);

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));

    bool powerModeSet = false;
    device.SetPowerMode(AccelerometerCore::PowerMode::powerDown, [&powerModeSet]()
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

TEST_F(Lsm303dlhcLifetimeTest, a_device_without_a_data_ready_pin_stops_cleanly)
{
    AccelerometerCore device{ bus };
    Initialize(device);

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcLifetimeTest, stop_defers_until_an_internally_issued_transaction_completes)
{
    AccelerometerCore device{ bus, dataReadyPin };
    Initialize(device);

    EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    bus.completeAutomatically = false;

    device.AsAccelerometer().Start([this](AccelerometerCore::Accelerometer::Samples)
        {
            bursts += 1;
        });

    ASSERT_TRUE(bus.CompletionPending());

    bool stopped = false;
    device.Stop([&stopped]()
        {
            stopped = true;
        });

    ExecuteAllActions();

    EXPECT_FALSE(stopped);

    bus.CompletePending();
    ExecuteAllActions();

    EXPECT_TRUE(stopped);
}

TEST_F(Lsm303dlhcLifetimeTest, the_magnetometer_stops_cleanly_while_streaming)
{
    MagnetometerCore device{ bus, dataReadyPin };
    Initialize(device);

    device.AsMagnetometer().Start([this](MagnetometerCore::Magnetometer::Samples)
        {
            bursts += 1;
        });

    ExecuteAllActions();

    infra::VerifyingFunction<void()> stopped;
    device.Stop(stopped);
    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, bursts);
}
