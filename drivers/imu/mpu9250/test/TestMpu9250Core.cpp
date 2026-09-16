#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "drivers/imu/mpu9250/test/Mpu9250BusAccessMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Acceleration = drivers::Mpu9250Core::Acceleration;
    using AngularVelocity = drivers::Mpu9250Core::AngularVelocity;
    using InitializationResult = drivers::Mpu9250Core::InitializationResult;
    using PowerMode = drivers::Mpu9250Core::PowerMode;

    class Mpu9250CoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Mpu9250CoreTest()
        {
            EXPECT_CALL(bus, RequiresI2cSlaveInterfaceDisabled()).WillRepeatedly(testing::Return(false));
        }

        void ExpectWhoAmI(uint8_t value)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x75, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ value }));
        }

        void ExpectConfigurationWrites()
        {
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
        }

        void Initialize(const drivers::Mpu9250Core::Config& config = drivers::Mpu9250Core::Config())
        {
            ExpectWhoAmI(0x71);
            ExpectConfigurationWrites();

            device.Initialize(config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(101));
        }

        std::vector<uint8_t> Measurement(int16_t accelerationX, int16_t accelerationY, int16_t accelerationZ,
            int16_t temperature, int16_t angularVelocityX, int16_t angularVelocityY, int16_t angularVelocityZ)
        {
            std::vector<uint8_t> data;

            for (int16_t sample : { accelerationX, accelerationY, accelerationZ, temperature, angularVelocityX, angularVelocityY, angularVelocityZ })
            {
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(sample) >> 8));
                data.push_back(static_cast<uint8_t>(sample & 0xff));
            }

            return data;
        }

        testing::StrictMock<drivers::Mpu9250BusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        drivers::Mpu9250Core device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
    };
}

TEST_F(Mpu9250CoreTest, initialize_reads_who_am_i_and_reports_success)
{
    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Mpu9250CoreTest, initialize_reports_device_not_found_on_who_am_i_mismatch)
{
    ExpectWhoAmI(0x68);

    device.Initialize(drivers::Mpu9250Core::Config(), [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ExecuteAllActions();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Mpu9250CoreTest, initialize_accepts_a_configured_alternative_who_am_i)
{
    drivers::Mpu9250Core::Config config;
    config.expectedWhoAmI = 0x73;

    ExpectWhoAmI(0x73);
    ExpectConfigurationWrites();

    device.Initialize(config, [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(101));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Mpu9250CoreTest, initialize_does_not_complete_before_the_reset_delay_has_elapsed)
{
    ExpectWhoAmI(0x71);
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));

    device.Initialize(drivers::Mpu9250Core::Config(), [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(99));

    EXPECT_FALSE(initializationResult);
}

TEST_F(Mpu9250CoreTest, initialize_programs_the_configured_scales_and_filters)
{
    drivers::Mpu9250Core::Config config;
    config.accelerometerFullScale = drivers::Mpu9250Core::AccelerometerFullScale::g16;
    config.gyroscopeFullScale = drivers::Mpu9250Core::GyroscopeFullScale::dps2000;
    config.gyroscopeLowPassFilter = drivers::Mpu9250Core::GyroscopeLowPassFilter::bandwidth20Hz;
    config.accelerometerLowPassFilter = drivers::Mpu9250Core::AccelerometerLowPassFilter::bandwidth21Hz;
    config.sampleRateDivider = 9;

    ExpectWhoAmI(0x71);
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x09 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x18 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x18 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x37, std::vector<uint8_t>{ 0x10 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

    device.Initialize(config, [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(101));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Mpu9250CoreTest, initialize_encodes_the_interrupt_pin_configuration)
{
    drivers::Mpu9250Core::Config config;
    config.interruptPolarity = drivers::Mpu9250Core::InterruptPolarity::activeLow;
    config.interruptDrive = drivers::Mpu9250Core::InterruptDrive::openDrain;
    config.interruptLatch = drivers::Mpu9250Core::InterruptLatch::latchedUntilCleared;
    config.clearInterruptOnAnyRead = false;

    ExpectWhoAmI(0x71);
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1a, std::vector<uint8_t>{ 0x03 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x03 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x37, std::vector<uint8_t>{ 0xe0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

    device.Initialize(config, [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(101));

    EXPECT_TRUE(initializationResult);
}

TEST_F(Mpu9250CoreTest, start_enables_the_raw_data_ready_interrupt)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.AsAccelerometer().Start([](drivers::Mpu9250Core::Accelerometer::Samples) {});

    ExecuteAllActions();
}

TEST_F(Mpu9250CoreTest, starting_both_sensors_enables_the_interrupt_only_once)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.AsAccelerometer().Start([](drivers::Mpu9250Core::Accelerometer::Samples) {});
    device.AsGyroscope().Start([](drivers::Mpu9250Core::Gyroscope::Samples) {});

    ExecuteAllActions();
}

TEST_F(Mpu9250CoreTest, data_ready_interrupt_delivers_three_acceleration_samples)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    std::vector<int32_t> received;
    device.AsAccelerometer().Start([&received](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                received.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(16384, -16384, 8192, 0, 0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(9807, received[0]);
    EXPECT_EQ(-9807, received[1]);
    EXPECT_EQ(4903, received[2]);
}

TEST_F(Mpu9250CoreTest, data_ready_interrupt_delivers_three_angular_velocity_samples)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    std::vector<int32_t> received;
    device.AsGyroscope().Start([&received](drivers::Mpu9250Core::Gyroscope::Samples samples)
        {
            for (auto sample : samples)
                received.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(0, 0, 0, 0, 16384, -16384, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(125000, received[0]);
    EXPECT_EQ(-125000, received[1]);
    EXPECT_EQ(0, received[2]);
}

TEST_F(Mpu9250CoreTest, the_callback_stays_registered_across_consecutive_bursts)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    int bursts = 0;
    device.AsAccelerometer().Start([&bursts](drivers::Mpu9250Core::Accelerometer::Samples)
        {
            ++bursts;
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).Times(2).WillRepeatedly(testing::Return(Measurement(0, 0, 0, 0, 0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
    dataReadyPin.SetStubState(false);
    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(2, bursts);
}

TEST_F(Mpu9250CoreTest, stopping_the_accelerometer_leaves_the_gyroscope_running)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    int accelerationBursts = 0;
    int angularVelocityBursts = 0;

    device.AsAccelerometer().Start([&accelerationBursts](drivers::Mpu9250Core::Accelerometer::Samples)
        {
            ++accelerationBursts;
        });
    device.AsGyroscope().Start([&angularVelocityBursts](drivers::Mpu9250Core::Gyroscope::Samples)
        {
            ++angularVelocityBursts;
        });

    ExecuteAllActions();

    device.AsAccelerometer().Stop();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(0, 0, 0, 0, 0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(0, accelerationBursts);
    EXPECT_EQ(1, angularVelocityBursts);
}

TEST_F(Mpu9250CoreTest, stopping_the_last_sensor_disables_the_interrupt)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.AsAccelerometer().Start([](drivers::Mpu9250Core::Accelerometer::Samples) {});

    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

    device.AsAccelerometer().Stop();

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
}

TEST_F(Mpu9250CoreTest, measure_temperature_converts_the_raw_reading)
{
    Initialize();

    EXPECT_CALL(bus, ReadRegisterMock(0x41, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x00 }));

    std::optional<int32_t> temperature;
    device.MeasureTemperature([&temperature](drivers::Mpu9250Core::Temperature value)
        {
            temperature = value.Value();
        });

    ExecuteAllActions();

    ASSERT_TRUE(temperature);
    EXPECT_EQ(21000, *temperature);
}

TEST_F(Mpu9250CoreTest, set_power_mode_sleep_sets_the_sleep_bit)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x41 }));

    infra::VerifyingFunction<void()> done;
    device.SetPowerMode(PowerMode::sleep, done);

    ExecuteAllActions();

    EXPECT_EQ(PowerMode::sleep, device.CurrentPowerMode());
}

TEST_F(Mpu9250CoreTest, set_power_mode_standby_sets_gyroscope_standby_and_disables_all_axes)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x3f }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x11 }));

    infra::VerifyingFunction<void()> done;
    device.SetPowerMode(PowerMode::standby, done);

    ExecuteAllActions();

    EXPECT_EQ(PowerMode::standby, device.CurrentPowerMode());
}

TEST_F(Mpu9250CoreTest, set_power_mode_normal_clears_sleep_and_standby_and_preserves_the_clock_source)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x3f }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x11 }));

    infra::VerifyingFunction<void()> toStandby;
    device.SetPowerMode(PowerMode::standby, toStandby);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));

    infra::VerifyingFunction<void()> toNormal;
    device.SetPowerMode(PowerMode::normal, toNormal);
    ExecuteAllActions();

    EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
}

TEST_F(Mpu9250CoreTest, waking_from_sleep_waits_for_the_gyroscope_start_up_time)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x41 }));

    infra::VerifyingFunction<void()> toSleep;
    device.SetPowerMode(PowerMode::sleep, toSleep);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));

    bool woken = false;
    device.SetPowerMode(PowerMode::normal, [&woken]()
        {
            woken = true;
        });

    ForwardTime(std::chrono::milliseconds(34));
    EXPECT_FALSE(woken);

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(woken);
}

TEST_F(Mpu9250CoreTest, set_accelerometer_full_scale_writes_the_scale_and_applies_to_later_samples)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x18 }));

    infra::VerifyingFunction<void()> done;
    device.SetAccelerometerFullScale(drivers::Mpu9250Core::AccelerometerFullScale::g16, done);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    std::vector<int32_t> received;
    device.AsAccelerometer().Start([&received](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                received.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(2048, 0, 0, 0, 0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(9807, received[0]);
}

TEST_F(Mpu9250CoreTest, no_interrupt_is_armed_when_no_data_ready_pin_is_wired)
{
    drivers::Mpu9250Core deviceWithoutPin{ bus };

    ExpectWhoAmI(0x71);
    ExpectConfigurationWrites();

    deviceWithoutPin.Initialize(drivers::Mpu9250Core::Config(), [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(101));

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    deviceWithoutPin.AsAccelerometer().Start([](drivers::Mpu9250Core::Accelerometer::Samples) {});

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
}

TEST_F(Mpu9250CoreTest, acceleration_at_sixteen_g_full_scale_does_not_overflow)
{
    drivers::Mpu9250Core::Config config;
    config.accelerometerFullScale = drivers::Mpu9250Core::AccelerometerFullScale::g16;

    ExpectWhoAmI(0x71);
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1a, std::vector<uint8_t>{ 0x03 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x18 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x03 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x37, std::vector<uint8_t>{ 0x10 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

    device.Initialize(config, [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(101));

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    std::vector<int32_t> received;
    device.AsAccelerometer().Start([&received](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                received.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(32767, -32768, 0, 0, 0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(156902, received[0]);
    EXPECT_EQ(-156906, received[1]);
    EXPECT_EQ(0, received[2]);
}

TEST_F(Mpu9250CoreTest, angular_velocity_at_two_thousand_dps_full_scale_does_not_overflow)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x18 }));

    infra::VerifyingFunction<void()> scaleSet;
    device.SetGyroscopeFullScale(drivers::Mpu9250Core::GyroscopeFullScale::dps2000, scaleSet);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    std::vector<int32_t> received;
    device.AsGyroscope().Start([&received](drivers::Mpu9250Core::Gyroscope::Samples samples)
        {
            for (auto sample : samples)
                received.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Measurement(0, 0, 0, 0, 32767, -32768, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(1999939, received[0]);
    EXPECT_EQ(-2000000, received[1]);
    EXPECT_EQ(0, received[2]);
}

TEST_F(Mpu9250CoreTest, temperature_conversion_handles_negative_readings)
{
    Initialize();

    EXPECT_CALL(bus, ReadRegisterMock(0x41, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xe5, 0xd9 }));

    std::optional<int32_t> temperature;
    device.MeasureTemperature([&temperature](drivers::Mpu9250Core::Temperature value)
        {
            temperature = value.Value();
        });

    ExecuteAllActions();

    ASSERT_TRUE(temperature);
    EXPECT_EQ(947, *temperature);
}
