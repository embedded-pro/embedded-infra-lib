#include "drivers/imu/l3gd20/L3gd20Core.hpp"
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
    using Device = drivers::L3gd20Core;
    using FullScale = Device::FullScale;
    using InitializationResult = Device::InitializationResult;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;
    using Variant = Device::Variant;

    class L3gd20CoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static Device::Config DefaultConfig(Variant variant)
        {
            Device::Config config;
            config.variant = variant;
            config.turnOnTime = std::chrono::milliseconds(5);

            return config;
        }

        void ExpectReboot(uint8_t identification)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ identification }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
        }

        void ExpectConfiguration(Variant variant, uint8_t control1 = 0x0f, uint8_t control4 = 0x80, uint8_t control5 = 0x00, uint8_t lowOutputDataRate = 0x00)
        {
            if (variant == Variant::l3gd20h)
                EXPECT_CALL(bus, WriteRegisterMock(0x39, std::vector<uint8_t>{ lowOutputDataRate }));

            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ control4 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ control5 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ control1 }));
        }

        void Initialize(const Device::Config& config)
        {
            device.Initialize(config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(30));
        }

        void InitializeDefault(Variant variant = Variant::l3gd20)
        {
            ExpectReboot(variant == Variant::l3gd20h ? 0xd7 : 0xd4);
            ExpectConfiguration(variant);
            Initialize(DefaultConfig(variant));
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming()
        {
            ExpectModifyRegister(0x22, 0x00, 0x08);

            device.AsGyroscope().Start([this](Device::Gyroscope::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        // The output registers are little endian, low byte at the lower address
        static std::vector<uint8_t> Measurement(int16_t x, int16_t y, int16_t z)
        {
            std::vector<uint8_t> data;

            for (int16_t count : { x, y, z })
            {
                auto word = static_cast<uint16_t>(count);
                data.push_back(static_cast<uint8_t>(word & 0xff));
                data.push_back(static_cast<uint8_t>(word >> 8));
            }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        hal::GpioPinStub interruptPin;
        Device device{ bus, dataReadyPin, interruptPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
    };

    TEST_F(L3gd20CoreTest, initialize_reads_the_identification_and_reports_success)
    {
        InitializeDefault();

        EXPECT_EQ(InitializationResult::success, initializationResult);
        EXPECT_TRUE(device.Initialized());
    }

    TEST_F(L3gd20CoreTest, initialize_reports_device_not_found_on_an_identification_mismatch)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        Initialize(DefaultConfig(Variant::l3gd20));

        EXPECT_EQ(InitializationResult::deviceNotFound, initializationResult);
        EXPECT_FALSE(device.Initialized());
    }

    TEST_F(L3gd20CoreTest, the_l3gd20h_identification_is_rejected_by_an_l3gd20)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd7 }));

        Initialize(DefaultConfig(Variant::l3gd20));

        EXPECT_EQ(InitializationResult::deviceNotFound, initializationResult);
    }

    TEST_F(L3gd20CoreTest, the_l3gd20h_expects_its_own_identification_and_writes_the_low_output_data_rate_register)
    {
        InitializeDefault(Variant::l3gd20h);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_accepts_a_configured_alternative_identification)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.expectedIdentification = 0xd3;

        ExpectReboot(0xd3);
        ExpectConfiguration(Variant::l3gd20);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_does_not_complete_before_the_turn_on_time_has_elapsed)
    {
        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20);

        auto config = DefaultConfig(Variant::l3gd20);
        config.turnOnTime = std::chrono::milliseconds(250);

        device.Initialize(config, [this](InitializationResult result)
            {
                initializationResult = result;
            });

        ForwardTime(std::chrono::milliseconds(269));
        EXPECT_FALSE(initializationResult);

        ForwardTime(std::chrono::milliseconds(2));
        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_programs_the_configured_rate_bandwidth_and_scale)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.outputDataRate = OutputDataRate::hertz760;
        config.bandwidth = Device::Bandwidth::cutOff3;
        config.fullScale = FullScale::dps2000;

        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20, 0xff, 0xa0);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_selects_only_the_enabled_axes)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.enableY = false;

        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20, 0x0d);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_programs_an_active_low_open_drain_interrupt)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.interruptPolarity = Device::InterruptPolarity::activeLow;
        config.interruptDrive = Device::InterruptDrive::openDrain;

        ExpectReboot(0xd4);
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x30 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, initialize_sets_the_three_wire_serial_interface_bit_when_configured)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.threeWireSpi = true;

        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20, 0x0f, 0x81);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, the_l3gd20h_enables_the_low_output_data_rate_block)
    {
        auto config = DefaultConfig(Variant::l3gd20h);
        config.outputDataRate = OutputDataRate::hertz12_5;

        ExpectReboot(0xd7);
        ExpectConfiguration(Variant::l3gd20h, 0x0f, 0x80, 0x00, 0x01);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, the_l3gd20h_disables_the_i2c_interface_when_configured)
    {
        auto config = DefaultConfig(Variant::l3gd20h);
        config.disableI2cInterface = true;

        ExpectReboot(0xd7);
        ExpectConfiguration(Variant::l3gd20h, 0x0f, 0x80, 0x00, 0x10);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, the_high_pass_output_path_enables_the_high_pass_stage)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.outputSelection = Device::OutputSelection::highPass;

        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20, 0x0f, 0x80, 0x11);
        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(L3gd20CoreTest, setting_the_output_data_rate_rewrites_control_register_one)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x4f }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz190, done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, switching_into_the_low_output_data_rate_block_powers_down_first)
    {
        InitializeDefault(Variant::l3gd20h);

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x39, std::vector<uint8_t>{ 0x01 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x4f }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz25, done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, setting_the_bandwidth_preserves_the_output_data_rate)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x2f }));

        infra::VerifyingFunction<void()> done;
        device.SetBandwidth(Device::Bandwidth::cutOff2, done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, setting_the_full_scale_rewrites_control_register_four)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x90 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::dps500, done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, setting_the_high_pass_filter_rewrites_control_register_two)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x23 }));

        infra::VerifyingFunction<void()> done;
        device.SetHighPassFilter(Device::HighPassMode::normal, 3, done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, software_reset_sets_the_reset_bit_on_the_l3gd20h)
    {
        InitializeDefault(Variant::l3gd20h);

        EXPECT_CALL(bus, WriteRegisterMock(0x39, std::vector<uint8_t>{ 0x04 }));

        infra::VerifyingFunction<void()> done;
        device.SoftwareReset(done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, set_power_mode_sleep_keeps_the_device_powered_with_the_axes_disabled)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x08 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::sleep, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::sleep, device.CurrentPowerMode());
    }

    TEST_F(L3gd20CoreTest, set_power_mode_power_down_clears_the_power_bit)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::powerDown, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());
    }

    TEST_F(L3gd20CoreTest, waking_from_power_down_waits_for_the_turn_on_time)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

        bool done = false;
        device.SetPowerMode(PowerMode::normal, [&done]()
            {
                done = true;
            });

        ExecuteAllActions();
        EXPECT_FALSE(done);

        ForwardTime(std::chrono::milliseconds(6));
        EXPECT_TRUE(done);
    }

    TEST_F(L3gd20CoreTest, starting_the_gyroscope_enables_the_data_ready_interrupt_line)
    {
        InitializeDefault();
        StartStreaming();
    }

    TEST_F(L3gd20CoreTest, stopping_the_gyroscope_disables_the_data_ready_interrupt_line)
    {
        InitializeDefault();
        StartStreaming();

        ExpectModifyRegister(0x22, 0x08, 0x00);

        device.AsGyroscope().Stop();

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, enabling_the_data_ready_source_preserves_the_other_interrupt_sources)
    {
        InitializeDefault();

        ExpectModifyRegister(0x22, 0x04, 0x0c);

        device.AsGyroscope().Start([](Device::Gyroscope::Samples) {});

        ExecuteAllActions();
    }

    TEST_F(L3gd20CoreTest, a_data_ready_edge_reads_six_bytes_and_delivers_three_samples)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(100, -200, 300)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        // 8.75 milli-degrees per count at the 250 dps scale
        EXPECT_EQ((std::vector<int32_t>{ 875, -1750, 2625 }), received);
    }

    TEST_F(L3gd20CoreTest, the_callback_stays_registered_across_consecutive_bursts)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(0, 0, 8)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(0, 0, 16)));

        dataReadyPin.SetStubState(false);
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ((std::vector<int32_t>{ 0, 0, 70, 0, 0, 140 }), received);
    }

    TEST_F(L3gd20CoreTest, the_samples_are_scaled_by_the_full_scale_setting)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0xa0 }));
        device.SetFullScale(FullScale::dps2000, infra::emptyFunction);
        ExecuteAllActions();

        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(100, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        // 70 milli-degrees per count at the 2000 dps scale
        EXPECT_EQ((std::vector<int32_t>{ 7000, 0, 0 }), received);
    }

    TEST_F(L3gd20CoreTest, the_largest_reading_at_the_widest_scale_does_not_overflow)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0xa0 }));
        device.SetFullScale(FullScale::dps2000, infra::emptyFunction);
        ExecuteAllActions();

        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(32767, -32768, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ((std::vector<int32_t>{ 2293690, -2293760, 0 }), received);
    }

    TEST_F(L3gd20CoreTest, conversion_rounds_symmetrically_around_zero)
    {
        InitializeDefault();
        StartStreaming();

        // 8.75 milli-degrees per count leaves a half count to round on every odd reading
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Measurement(1, -1, 3)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ((std::vector<int32_t>{ 9, -9, 26 }), received);
    }

    TEST_F(L3gd20CoreTest, measure_temperature_converts_the_inverted_slope)
    {
        InitializeDefault();

        EXPECT_CALL(bus, ReadRegisterMock(0x26, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xfb }));

        std::optional<Device::Temperature> temperature;
        device.MeasureTemperature([&temperature](Device::Temperature measured)
            {
                temperature = measured;
            });

        ExecuteAllActions();

        // Five counts below the reference is five degrees above it
        EXPECT_EQ(30000, temperature->Value());
    }

    TEST_F(L3gd20CoreTest, the_temperature_reference_is_configurable)
    {
        auto config = DefaultConfig(Variant::l3gd20);
        config.temperatureReferenceMilliCelsius = 0;

        ExpectReboot(0xd4);
        ExpectConfiguration(Variant::l3gd20);
        Initialize(config);

        EXPECT_CALL(bus, ReadRegisterMock(0x26, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0a }));

        std::optional<Device::Temperature> temperature;
        device.MeasureTemperature([&temperature](Device::Temperature measured)
            {
                temperature = measured;
            });

        ExecuteAllActions();

        EXPECT_EQ(-10000, temperature->Value());
    }

    TEST_F(L3gd20CoreTest, a_device_without_a_data_ready_pin_arms_no_interrupt)
    {
        testing::StrictMock<services::RegisterBusAccessMock> pinlessBus;
        Device pinless{ pinlessBus };

        EXPECT_CALL(pinlessBus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

        pinless.Initialize(DefaultConfig(Variant::l3gd20), [](InitializationResult) {});
        ForwardTime(std::chrono::milliseconds(30));

        EXPECT_CALL(pinlessBus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(pinlessBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));

        pinless.AsGyroscope().Start([](Device::Gyroscope::Samples) {});
        ExecuteAllActions();

        infra::VerifyingFunction<void()> stopped;
        pinless.Stop(stopped);
        ExecuteAllActions();
    }
}
