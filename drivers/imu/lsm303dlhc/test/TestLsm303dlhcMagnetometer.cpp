#include "drivers/imu/lsm303dlhc/Lsm303dlhcMagnetometer.hpp"
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
    using Device = drivers::Lsm303dlhcMagnetometer;
    using Gain = Device::Gain;
    using InitializationResult = Device::InitializationResult;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;

    class Lsm303dlhcMagnetometerTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectIdentification(std::vector<uint8_t> identification = { 0x48, 0x34, 0x33 })
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x0a, 3)).WillOnce(testing::Return(identification));
        }

        void ExpectConfigurationWrites(uint8_t configurationA = 0x90, uint8_t configurationB = 0x20, uint8_t mode = 0x00)
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x00, std::vector<uint8_t>{ configurationA }));
            EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ configurationB }));
            EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ mode }));
        }

        void Initialize(const Device::Config& config = Device::Config())
        {
            device.Initialize(config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(6));
        }

        void StartStreaming()
        {
            device.AsMagnetometer().Start([this](Device::Magnetometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());

                    saturatedDuringCallback = device.Saturated();
                });

            ExecuteAllActions();
        }

        // The output registers run X, Z, Y and are big endian
        static std::vector<uint8_t> Measurement(int16_t x, int16_t y, int16_t z)
        {
            std::vector<uint8_t> data;

            for (int16_t count : { x, z, y })
            {
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(count) >> 8));
                data.push_back(static_cast<uint8_t>(count & 0xff));
            }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
        bool saturatedDuringCallback = false;
    };
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_reads_the_identification_registers_and_reports_success)
{
    ExpectIdentification();
    ExpectConfigurationWrites();

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_reports_device_not_found_on_an_identification_mismatch)
{
    ExpectIdentification({ 0x48, 0x34, 0x00 });

    Initialize();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_accepts_a_configured_alternative_identification)
{
    Device::Config config;
    config.expectedIdentification = { { 0x48, 0x34, 0x00 } };

    ExpectIdentification({ 0x48, 0x34, 0x00 });
    ExpectConfigurationWrites();

    Initialize(config);

    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_does_not_complete_before_the_turn_on_delay_has_elapsed)
{
    ExpectIdentification();
    ExpectConfigurationWrites();

    device.Initialize(Device::Config(), [this](InitializationResult result)
        {
            initializationResult = result;
        });

    ForwardTime(std::chrono::milliseconds(4));
    EXPECT_FALSE(initializationResult);

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(initializationResult);
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_programs_the_configured_gain_rate_and_mode)
{
    Device::Config config;
    config.gain = Gain::milliGauss8100;
    config.outputDataRate = OutputDataRate::milliHertz220000;
    config.mode = Device::Mode::single;

    ExpectIdentification();
    ExpectConfigurationWrites(0x9c, 0xe0, 0x01);

    Initialize(config);
}

TEST_F(Lsm303dlhcMagnetometerTest, initialize_leaves_the_temperature_sensor_disabled_when_configured)
{
    Device::Config config;
    config.temperatureEnabled = false;

    ExpectIdentification();
    ExpectConfigurationWrites(0x10);

    Initialize(config);
}

TEST_F(Lsm303dlhcMagnetometerTest, starting_the_magnetometer_writes_no_register)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    StartStreaming();
}

TEST_F(Lsm303dlhcMagnetometerTest, a_data_ready_edge_delivers_the_samples_reordered_to_x_y_z)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(Measurement(1100, 550, 980)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(1000, received[0]);
    EXPECT_EQ(500, received[1]);
    EXPECT_EQ(1000, received[2]);
}

TEST_F(Lsm303dlhcMagnetometerTest, the_z_axis_uses_its_own_sensitivity)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(Measurement(1100, 1100, 1100)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(1000, received[0]);
    EXPECT_EQ(1000, received[1]);
    EXPECT_EQ(1122, received[2]);
}

TEST_F(Lsm303dlhcMagnetometerTest, samples_are_scaled_by_the_gain_setting)
{
    Device::Config config;
    config.gain = Gain::milliGauss8100;

    ExpectIdentification();
    ExpectConfigurationWrites(0x90, 0xe0);
    Initialize(config);
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(Measurement(230, 0, 205)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(1000, received[0]);
    EXPECT_EQ(1000, received[2]);
}

TEST_F(Lsm303dlhcMagnetometerTest, negative_samples_round_symmetrically)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(Measurement(-1100, 1100, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, received.size());
    EXPECT_EQ(-1000, received[0]);
    EXPECT_EQ(1000, received[1]);
}

TEST_F(Lsm303dlhcMagnetometerTest, a_saturated_axis_is_reported)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(Measurement(0, -4096, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(saturatedDuringCallback);
}

TEST_F(Lsm303dlhcMagnetometerTest, saturation_is_cleared_on_the_next_unsaturated_measurement)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6))
        .WillOnce(testing::Return(Measurement(0, -4096, 0)))
        .WillOnce(testing::Return(Measurement(0, 0, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
    ASSERT_TRUE(saturatedDuringCallback);

    dataReadyPin.SetStubState(false);
    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_FALSE(saturatedDuringCallback);
}

TEST_F(Lsm303dlhcMagnetometerTest, setting_the_gain_rewrites_configuration_register_b)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x80 }));

    infra::VerifyingFunction<void()> done;
    device.SetGain(Gain::milliGauss4000, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcMagnetometerTest, setting_the_output_data_rate_preserves_the_temperature_enable_bit)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x00, std::vector<uint8_t>{ 0x98 }));

    infra::VerifyingFunction<void()> done;
    device.SetOutputDataRate(OutputDataRate::milliHertz75000, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcMagnetometerTest, sleep_mode_writes_the_sleep_code_to_the_mode_register)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x02 }));

    infra::VerifyingFunction<void()> done;
    device.SetPowerMode(PowerMode::sleep, done);

    ExecuteAllActions();

    EXPECT_EQ(PowerMode::sleep, device.CurrentPowerMode());
}

TEST_F(Lsm303dlhcMagnetometerTest, waking_from_sleep_restores_the_configured_mode_and_waits)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x02 }));
    device.SetPowerMode(PowerMode::sleep, infra::emptyFunction);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x00 }));

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

TEST_F(Lsm303dlhcMagnetometerTest, measure_temperature_converts_eight_counts_per_degree)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, ReadRegisterMock(0x31, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0c, 0x80 }));

    std::optional<int32_t> temperature;
    device.MeasureTemperature([&temperature](Device::Temperature value)
        {
            temperature = value.Value();
        });

    ExecuteAllActions();

    ASSERT_TRUE(temperature);
    EXPECT_EQ(25000, *temperature);
}

TEST_F(Lsm303dlhcMagnetometerTest, a_negative_temperature_is_converted)
{
    ExpectIdentification();
    ExpectConfigurationWrites();
    Initialize();

    EXPECT_CALL(bus, ReadRegisterMock(0x31, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xff, 0x00 }));

    std::optional<int32_t> temperature;
    device.MeasureTemperature([&temperature](Device::Temperature value)
        {
            temperature = value.Value();
        });

    ExecuteAllActions();

    ASSERT_TRUE(temperature);
    EXPECT_EQ(-2000, *temperature);
}
