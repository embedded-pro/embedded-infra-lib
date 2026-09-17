#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometer.hpp"
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
    using Device = drivers::Lsm303dlhcAccelerometer;
    using FullScale = Device::FullScale;
    using InitializationResult = Device::InitializationResult;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;

    class Lsm303dlhcAccelerometerTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectInitialization(uint8_t control1 = 0x57, uint8_t control4 = 0x88, uint8_t control5 = 0x00, uint8_t control6 = 0x00)
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ control1 }));
            EXPECT_CALL(bus, ReadRegisterMock(0x20, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ control1 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ control4 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ control5 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ control6 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x26, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        }

        void Initialize(const Device::Config& config = Device::Config())
        {
            device.Initialize(config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(7));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x10 }));

            device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        // The output is little endian and left justified, so a twelve bit count sits in the top bits
        static std::vector<uint8_t> Measurement(int16_t x, int16_t y, int16_t z)
        {
            std::vector<uint8_t> data;

            for (int16_t count : { x, y, z })
            {
                auto word = static_cast<uint16_t>(static_cast<uint16_t>(count) << 4);
                data.push_back(static_cast<uint8_t>(word & 0xff));
                data.push_back(static_cast<uint8_t>(word >> 8));
            }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
    };
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_boots_the_device_and_reports_success)
{
    ExpectInitialization();
    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_reports_device_not_found_when_the_control_register_does_not_read_back)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x20, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_does_not_complete_before_the_boot_delay_has_elapsed)
{
    ExpectInitialization();

    device.Initialize(Device::Config(), [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(5));
    EXPECT_FALSE(initializationResult);

    ForwardTime(std::chrono::milliseconds(2));
    EXPECT_TRUE(initializationResult);
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_programs_the_configured_rate_scale_and_resolution)
{
    Device::Config config;
    config.outputDataRate = OutputDataRate::hertz400;
    config.fullScale = FullScale::g16;
    config.highResolution = false;

    ExpectInitialization(0x77, 0xb0);
    Initialize(config);

    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_selects_only_the_enabled_axes)
{
    Device::Config config;
    config.enableY = false;

    ExpectInitialization(0x55);
    Initialize(config);
}

TEST_F(Lsm303dlhcAccelerometerTest, initialize_programs_an_active_low_latched_interrupt)
{
    Device::Config config;
    config.interruptPolarity = Device::InterruptPolarity::activeLow;
    config.latchInterrupt = true;

    ExpectInitialization(0x57, 0x88, 0x08, 0x02);
    Initialize(config);
}

TEST_F(Lsm303dlhcAccelerometerTest, starting_the_accelerometer_enables_the_data_ready_interrupt_line)
{
    ExpectInitialization();
    Initialize();

    StartStreaming();
}

TEST_F(Lsm303dlhcAccelerometerTest, stopping_the_accelerometer_disables_the_data_ready_interrupt_line)
{
    ExpectInitialization();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));

    device.AsAccelerometer().Stop();
    ExecuteAllActions();
}

TEST_F(Lsm303dlhcAccelerometerTest, a_data_ready_edge_reads_six_bytes_and_delivers_three_samples)
{
    ExpectInitialization();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(1000, -1000, 500)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(9807, received[0]);
    EXPECT_EQ(-9807, received[1]);
    EXPECT_EQ(4903, received[2]);
}

TEST_F(Lsm303dlhcAccelerometerTest, the_callback_stays_registered_across_consecutive_bursts)
{
    ExpectInitialization();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).Times(2).WillRepeatedly(testing::Return(Measurement(1000, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
    dataReadyPin.SetStubState(false);
    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(6u, received.size());
}

TEST_F(Lsm303dlhcAccelerometerTest, samples_are_scaled_by_the_full_scale_setting)
{
    Device::Config config;
    config.fullScale = FullScale::g16;

    ExpectInitialization(0x57, 0xb8);
    Initialize(config);
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(1000, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(117680, received[0]);
}

TEST_F(Lsm303dlhcAccelerometerTest, a_full_scale_reading_at_sixteen_g_does_not_overflow)
{
    Device::Config config;
    config.fullScale = FullScale::g16;

    ExpectInitialization(0x57, 0xb8);
    Initialize(config);
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(2047, -2048, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(240891, received[0]);
    EXPECT_EQ(-241008, received[1]);
}

TEST_F(Lsm303dlhcAccelerometerTest, setting_the_full_scale_rewrites_control_register_four)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x98 }));

    infra::VerifyingFunction<void()> done;
    device.SetFullScale(FullScale::g4, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcAccelerometerTest, setting_the_output_data_rate_rewrites_control_register_one)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x27 }));

    infra::VerifyingFunction<void()> done;
    device.SetOutputDataRate(OutputDataRate::hertz10, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcAccelerometerTest, setting_high_resolution_rewrites_control_register_four)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));

    infra::VerifyingFunction<void()> done;
    device.SetHighResolution(false, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcAccelerometerTest, low_power_mode_sets_the_low_power_bit_and_clears_high_resolution)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x5f }));
    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));

    infra::VerifyingFunction<void()> done;
    device.SetPowerMode(PowerMode::lowPower, done);

    ExecuteAllActions();

    EXPECT_EQ(PowerMode::lowPower, device.CurrentPowerMode());
}

TEST_F(Lsm303dlhcAccelerometerTest, power_down_clears_the_output_data_rate)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));

    infra::VerifyingFunction<void()> done;
    device.SetPowerMode(PowerMode::powerDown, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcAccelerometerTest, waking_from_power_down_waits_for_the_turn_on_time)
{
    ExpectInitialization();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));

    device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));

    bool awake = false;
    device.SetPowerMode(PowerMode::normal, [&awake]()
        {
            awake = true;
        });

    ForwardTime(std::chrono::milliseconds(4));
    EXPECT_FALSE(awake);

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(awake);
}

TEST_F(Lsm303dlhcAccelerometerTest, no_interrupt_is_armed_when_no_data_ready_pin_is_wired)
{
    Device deviceWithoutPin{ bus };

    ExpectInitialization();
    deviceWithoutPin.Initialize(Device::Config(), [](InitializationResult) {});
    ForwardTime(std::chrono::milliseconds(7));

    EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x10 }));

    deviceWithoutPin.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
        {
            received.push_back(0);
        });

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(received.empty());

    infra::VerifyingFunction<void()> stopped;
    deviceWithoutPin.Stop(stopped);
    ExecuteAllActions();
}
