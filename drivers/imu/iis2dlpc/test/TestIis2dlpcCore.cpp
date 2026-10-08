#include "drivers/imu/common/SensorWithPolling.hpp"
#include "drivers/imu/iis2dlpc/Iis2dlpcCore.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Device = drivers::Iis2dlpcCore;
    using FullScale = Device::FullScale;
    using InitializationResult = Device::InitializationResult;
    using OperatingMode = Device::OperatingMode;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;

    class Iis2dlpcCoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectDefaultInit()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));
        }

        void Initialize(const Device::Config& config = {})
        {
            device.Initialize(config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });

            ForwardTime(std::chrono::milliseconds(15));
        }

        void InitializeDefault()
        {
            ExpectDefaultInit();
            Initialize();
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming()
        {
            ExpectModifyRegister(0x23, 0x00, 0x01);

            device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        static std::vector<uint8_t> Measurement(int16_t x, int16_t y, int16_t z)
        {
            std::vector<uint8_t> data;

            for (int16_t word : { x, y, z })
            {
                auto uword = static_cast<uint16_t>(word);
                data.push_back(static_cast<uint8_t>(uword & 0xff));
                data.push_back(static_cast<uint8_t>(uword >> 8));
            }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
    };

    TEST_F(Iis2dlpcCoreTest, default_initialize_issues_exact_register_sequence_and_reports_success)
    {
        InitializeDefault();

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Iis2dlpcCoreTest, wrong_who_am_i_reports_device_not_found_and_writes_nothing)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        Initialize();

        EXPECT_EQ(InitializationResult::deviceNotFound, initializationResult);
        EXPECT_FALSE(device.Initialized());
    }

    TEST_F(Iis2dlpcCoreTest, non_default_config_lowpower1_writes_correct_control_register_values)
    {
        // CTRL1 = hertz50(4)<<4 | lowPower1(0) = 0x40
        // CTRL2 = addressIncrement(0x04) only (blockDataUpdate off)
        // CTRL3 = interruptActiveLow(0x08)
        // CTRL6 = odrDividedBy10(2)<<6 | g8(2)<<4 | lowNoiseEnable(0x04) = 0x80|0x20|0x04 = 0xa4
        Device::Config config;
        config.outputDataRate = OutputDataRate::hertz50;
        config.operatingMode = OperatingMode::lowPower1;
        config.fullScale = FullScale::g8;
        config.filterBandwidth = Device::FilterBandwidth::odrDividedBy10;
        config.lowNoise = true;
        config.blockDataUpdate = false;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x04 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0xa4 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x40 }));

        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Iis2dlpcCoreTest, non_default_config_lowpower4_writes_lp_mode_3)
    {
        // CTRL1 = hertz50(4)<<4 | lowPower4(3) = 0x43
        Device::Config config;
        config.outputDataRate = OutputDataRate::hertz50;
        config.operatingMode = OperatingMode::lowPower4;
        config.fullScale = FullScale::g8;
        config.filterBandwidth = Device::FilterBandwidth::odrDividedBy10;
        config.lowNoise = true;
        config.blockDataUpdate = false;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x04 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0xa4 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x43 }));

        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Iis2dlpcCoreTest, initialized_is_false_before_and_true_after_initialize)
    {
        EXPECT_FALSE(device.Initialized());

        InitializeDefault();

        EXPECT_TRUE(device.Initialized());
    }

    TEST_F(Iis2dlpcCoreTest, current_power_mode_is_power_down_before_and_normal_after_initialize)
    {
        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());

        InitializeDefault();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Iis2dlpcCoreTest, set_power_mode_power_down_writes_ctrl1_with_odr_zero_and_mode_bits_kept)
    {
        // powerDown: ODR=0, highPerformance modeBits=0x04 -> 0x04
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::powerDown, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());
    }

    TEST_F(Iis2dlpcCoreTest, set_power_mode_normal_restores_configured_odr_and_mode_bits)
    {
        // normal: hertz100(5)<<4 | highPerformance(0x04) = 0x54
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::normal, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Iis2dlpcCoreTest, set_output_data_rate_writes_ctrl1_with_new_odr)
    {
        // hertz200=6: (6<<4) | 0x04 = 0x64
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x64 }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz200, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, set_output_data_rate_honours_power_down_mode)
    {
        // powerDown: ODR=0 regardless of rate: 0x04 (highPerformance bits only)
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz200, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, set_operating_mode_writes_ctrl1_with_new_mode_bits)
    {
        // lowPower1=0: (hertz100=5)<<4 | 0x00 = 0x50
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x50 }));

        infra::VerifyingFunction<void()> done;
        device.SetOperatingMode(OperatingMode::lowPower1, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, set_operating_mode_honours_power_down_mode)
    {
        // powerDown with lowPower1: (0<<4) | 0x00 = 0x00
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));

        infra::VerifyingFunction<void()> done;
        device.SetOperatingMode(OperatingMode::lowPower1, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, set_full_scale_writes_ctrl6_with_new_scale)
    {
        // g4=1: (0<<6)|(1<<4)|0 = 0x10
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::g4, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, set_full_scale_honours_power_down_mode)
    {
        // SetFullScale writes CTRL6 independent of power mode (g4 = 0x10 always)
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x04 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::g4, done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, start_accelerometer_enables_drdy_via_modify_with_current_zero)
    {
        InitializeDefault();

        ExpectModifyRegister(0x23, 0x00, 0x01);

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, start_accelerometer_enables_drdy_via_modify_with_nonzero_current)
    {
        InitializeDefault();

        // current 0x40 -> 0x41 (set bit 0, preserve other bits)
        ExpectModifyRegister(0x23, 0x40, 0x41);

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, start_accelerometer_arms_rising_edge_and_data_ready_triggers_read)
    {
        InitializeDefault();

        ExpectModifyRegister(0x23, 0x00, 0x01);

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, active_low_polarity_arms_falling_edge)
    {
        Device::Config config;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        Initialize(config);

        EXPECT_CALL(bus, ReadRegisterMock(0x23, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x01 }));

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        dataReadyPin.SetStubState(true);

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(false);
        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_samples_at_g2_14bit)
    {
        // g2, 14-bit: 244 ug/count; 1000 counts -> 244000 ug -> 2393 mm/s^2
        // raw = 1000 << 2 = 4000 (non-zero low bits: 4003 = 4000|3, proves low bits discarded)
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(4003), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(2393, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_samples_at_g4_14bit)
    {
        // g4, 14-bit: 488 ug/count; 1000 counts -> 488000 ug -> 4786 mm/s^2
        Device::Config config;
        config.fullScale = FullScale::g4;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x00, 0x01);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(4000), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(4786, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_samples_at_g8_14bit)
    {
        // g8, 14-bit: 976 ug/count; 1000 counts -> 976000 ug -> 9571 mm/s^2
        Device::Config config;
        config.fullScale = FullScale::g8;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x20 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x00, 0x01);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(4000), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(9571, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_samples_at_g16_14bit)
    {
        // g16, 14-bit: 1952 ug/count; 1000 counts -> 1952000 ug -> 19143 mm/s^2
        Device::Config config;
        config.fullScale = FullScale::g16;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x30 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x00, 0x01);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(4000), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(19143, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_negative_sample_correctly)
    {
        // g2, 14-bit: -1000 counts -> -2393 mm/s^2
        // raw = -1000 << 2 = -4000 as int16_t
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(-4000), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(-2393, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_samples_in_lowpower1_12bit_mode)
    {
        // lowPower1: shift=4, ug/count=244*4=976; 1000 counts -> 976000 ug -> 9571 mm/s^2
        // raw = 1000 << 4 = 16000; add non-zero low nibble: 16015 = 16000|15 to prove discarded
        Device::Config config;
        config.operatingMode = OperatingMode::lowPower1;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x50 }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x00, 0x01);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(static_cast<int16_t>(16015), 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_FALSE(received.empty());
        EXPECT_EQ(9571, received[0]);
    }

    TEST_F(Iis2dlpcCoreTest, data_ready_edge_delivers_all_three_axis_samples)
    {
        // g2 14-bit: x=1000->2393, y=500->1196, z=-200->-479
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(
                static_cast<int16_t>(1000 << 2),
                static_cast<int16_t>(500 << 2),
                static_cast<int16_t>(-200 << 2))));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_EQ(3u, received.size());
        EXPECT_EQ(2393, received[0]);
        EXPECT_EQ(1196, received[1]);
        EXPECT_EQ(-479, received[2]);
    }

    TEST_F(Iis2dlpcCoreTest, stop_accelerometer_clears_drdy_interrupt_bit)
    {
        InitializeDefault();
        StartStreaming();

        ExpectModifyRegister(0x23, 0x01, 0x00);

        device.AsAccelerometer().Stop();

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcCoreTest, stop_completes_via_callback_after_outstanding_transaction)
    {
        InitializeDefault();
        StartStreaming();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(bus.CompletionPending());

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

    using PolledDevice = drivers::SensorWithPolling<drivers::Iis2dlpcCore>;

    class Iis2dlpcWithPollingTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void InitializeDefault()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x0c }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

            device.Initialize({}, [](InitializationResult) {});
            ForwardTime(std::chrono::milliseconds(15));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x23, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x01 }));

            device.AsAccelerometer().Start([this](PolledDevice::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        PolledDevice device{ bus };
        std::vector<int32_t> received;
    };

    TEST_F(Iis2dlpcWithPollingTest, poll_reads_status_register_and_delivers_when_data_available)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        ForwardTime(std::chrono::milliseconds(5));
    }

    TEST_F(Iis2dlpcWithPollingTest, poll_reads_status_but_skips_sample_when_data_not_ready)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(received.empty());
    }
}
