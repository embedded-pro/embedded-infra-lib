#include "drivers/imu/common/SensorWithPolling.hpp"
#include "drivers/imu/lis302dl/Lis302dlCore.hpp"
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
    using Device = drivers::Lis302dlCore;
    using FullScale = Device::FullScale;
    using InitializationResult = Device::InitializationResult;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;

    class Lis302dlCoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectDefaultInit()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x47 }));
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
            ExpectModifyRegister(0x22, 0x00, 0x04);

            device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        static std::vector<uint8_t> MeasurementWithGarbage(int8_t x, int8_t y, int8_t z)
        {
            return {
                static_cast<uint8_t>(x), 0xff,
                static_cast<uint8_t>(y), 0xab,
                static_cast<uint8_t>(z), 0x12
            };
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
    };

    TEST_F(Lis302dlCoreTest, default_initialize_issues_exact_register_sequence_and_reports_success)
    {
        InitializeDefault();

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Lis302dlCoreTest, wrong_who_am_i_reports_device_not_found_and_writes_nothing)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        Initialize();

        EXPECT_EQ(InitializationResult::deviceNotFound, initializationResult);
        EXPECT_FALSE(device.Initialized());
    }

    TEST_F(Lis302dlCoreTest, non_default_config_writes_correct_control_register_values)
    {
        // hertz400, g8, X and Z only, activeLow
        // CTRL1(normal) = 0x80|0x40|0x20|0x01|0x04 = 0xe5
        // CTRL3 = 0x80 (interruptActiveLow)
        Device::Config config;
        config.outputDataRate = OutputDataRate::hertz400;
        config.fullScale = FullScale::g8;
        config.enableX = true;
        config.enableY = false;
        config.enableZ = true;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0xe5 }));

        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Lis302dlCoreTest, initialized_is_false_before_and_true_after_initialize)
    {
        EXPECT_FALSE(device.Initialized());

        InitializeDefault();

        EXPECT_TRUE(device.Initialized());
    }

    TEST_F(Lis302dlCoreTest, current_power_mode_is_power_down_before_and_normal_after_initialize)
    {
        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());

        InitializeDefault();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Lis302dlCoreTest, set_power_mode_power_down_writes_ctrl1_with_active_bit_clear)
    {
        InitializeDefault();

        // powerDown: 0 (hertz100) | 0 (g2) | 0 (no active) | 0x07 (XYZ) = 0x07
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::powerDown, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());
    }

    TEST_F(Lis302dlCoreTest, set_power_mode_normal_restores_active_bit)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        // normal: 0 (hertz100) | 0x40 (active) | 0 (g2) | 0x07 (XYZ) = 0x47
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x47 }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::normal, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Lis302dlCoreTest, set_output_data_rate_writes_ctrl1_with_new_rate)
    {
        InitializeDefault();

        // hertz400: 0x80 | 0x40 (active) | 0 (g2) | 0x07 (XYZ) = 0xc7
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0xc7 }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz400, done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, set_output_data_rate_honours_power_down_mode)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        // powerDown with hertz400: 0x80 (rate) | 0 (no active) | 0 (g2) | 0x07 (XYZ) = 0x87
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x87 }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz400, done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, set_full_scale_writes_ctrl1_with_new_scale)
    {
        InitializeDefault();

        // g8: 0 (hertz100) | 0x40 (active) | 0x20 (g8) | 0x07 (XYZ) = 0x67
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x67 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::g8, done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, set_full_scale_honours_power_down_mode)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x07 }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        // powerDown with g8: 0 (hertz100) | 0 (no active) | 0x20 (g8) | 0x07 (XYZ) = 0x27
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x27 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::g8, done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, start_accelerometer_enables_data_ready_via_modify_register_and_uses_rising_edge)
    {
        InitializeDefault();

        // After init (activeHigh): CTRL3 = 0x00; EnableDataReadyInterrupt(true): set 0x04 -> 0x04
        ExpectModifyRegister(0x22, 0x00, 0x04);

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        // risingEdge: pin goes high -> triggers read
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, active_low_polarity_enables_data_ready_and_arms_falling_edge)
    {
        Device::Config config;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x47 }));

        Initialize(config);

        // After init (activeLow): CTRL3 = 0x80; EnableDataReadyInterrupt(true): set 0x04 -> 0x84
        EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x84 }));

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        // fallingEdge: must go high first, then low to trigger
        dataReadyPin.SetStubState(true);

        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(false);
        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, data_ready_edge_reads_samples_from_bytes_0_2_4_only_and_delivers_at_g2)
    {
        InitializeDefault();
        StartStreaming();

        // g2 (18000 µg/count): 100 -> 17652, 50 -> 8826, -50 -> -8826
        // Non-zero garbage in bytes 1, 3, 5 to prove they are ignored
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(MeasurementWithGarbage(100, 50, -50)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_EQ(3u, received.size());
        EXPECT_EQ(17652, received[0]);
        EXPECT_EQ(8826, received[1]);
        EXPECT_EQ(-8826, received[2]);
    }

    TEST_F(Lis302dlCoreTest, data_ready_edge_delivers_negative_sample_correctly)
    {
        InitializeDefault();
        StartStreaming();

        // g2 (18000 µg/count): -100 -> -17652
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(MeasurementWithGarbage(-100, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(-17652, received[0]);
    }

    TEST_F(Lis302dlCoreTest, data_ready_edge_delivers_samples_at_g8)
    {
        Device::Config config;
        config.fullScale = FullScale::g8;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x67 }));

        Initialize(config);

        ExpectModifyRegister(0x22, 0x00, 0x04);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        // g8 (72000 µg/count): 100 -> 70608
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(MeasurementWithGarbage(100, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(70608, received[0]);
    }

    TEST_F(Lis302dlCoreTest, data_ready_edge_delivers_maximum_int8_value)
    {
        InitializeDefault();
        StartStreaming();

        // g2 (18000 µg/count): 127 -> 22418
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(MeasurementWithGarbage(127, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(22418, received[0]);
    }

    TEST_F(Lis302dlCoreTest, data_ready_edge_delivers_minimum_int8_value)
    {
        InitializeDefault();
        StartStreaming();

        // g2 (18000 µg/count): -128 -> -22595
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(MeasurementWithGarbage(-128, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(-22595, received[0]);
    }

    TEST_F(Lis302dlCoreTest, stop_accelerometer_clears_data_ready_interrupt_bits)
    {
        InitializeDefault();
        StartStreaming();

        // After StartStreaming (activeHigh): CTRL3 = 0x04; clear I1CFG -> 0x00
        ExpectModifyRegister(0x22, 0x04, 0x00);

        device.AsAccelerometer().Stop();

        ExecuteAllActions();
    }

    TEST_F(Lis302dlCoreTest, stop_completes_via_callback_after_outstanding_transaction)
    {
        InitializeDefault();
        StartStreaming();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));
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

    using PolledDevice = drivers::SensorWithPolling<drivers::Lis302dlCore>;

    class Lis302dlWithPollingTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void InitializeDefault()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x47 }));

            device.Initialize({}, [](InitializationResult) {});
            ForwardTime(std::chrono::milliseconds(15));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x04 }));

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

    TEST_F(Lis302dlWithPollingTest, poll_reads_status_register_and_delivers_when_data_available)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x29, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        ForwardTime(std::chrono::milliseconds(5));
    }

    TEST_F(Lis302dlWithPollingTest, poll_reads_status_but_skips_sample_when_data_not_ready)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(received.empty());
    }
}
