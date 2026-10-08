#include "drivers/imu/common/SensorWithPolling.hpp"
#include "drivers/imu/lis3dsh/Lis3dshCore.hpp"
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
    using Device = drivers::Lis3dshCore;
    using FullScale = Device::FullScale;
    using InitializationResult = Device::InitializationResult;
    using OutputDataRate = Device::OutputDataRate;
    using PowerMode = Device::PowerMode;

    class Lis3dshCoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectDefaultInit()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));
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
            ExpectModifyRegister(0x23, 0x40, 0xc0);

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
        Device device{ bus, dataReadyPin };
        std::optional<InitializationResult> initializationResult;
        std::vector<int32_t> received;
    };

    TEST_F(Lis3dshCoreTest, default_initialize_issues_exact_register_sequence_and_reports_success)
    {
        InitializeDefault();

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Lis3dshCoreTest, wrong_who_am_i_reports_device_not_found_and_writes_nothing)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        Initialize();

        EXPECT_EQ(InitializationResult::deviceNotFound, initializationResult);
        EXPECT_FALSE(device.Initialized());
    }

    TEST_F(Lis3dshCoreTest, initialized_is_false_before_and_true_after_initialize)
    {
        EXPECT_FALSE(device.Initialized());

        InitializeDefault();

        EXPECT_TRUE(device.Initialized());
    }

    TEST_F(Lis3dshCoreTest, current_power_mode_is_power_down_before_and_normal_after_initialize)
    {
        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());

        InitializeDefault();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Lis3dshCoreTest, non_default_config_writes_correct_control_register_values)
    {
        // hertz400=7, g8=3, bandwidth hertz50=3, X/Z only, activeLow, blockDataUpdate off
        // Control4(normal) = (7<<4)|0|(0x01|0x04) = 0x70|0x05 = 0x75
        // Control5 = (3<<6)|(3<<3) = 0xC0|0x18 = 0xD8
        // Control3 = 0 (activeLow -> no IEA bit)
        Device::Config config;
        config.outputDataRate = OutputDataRate::hertz400;
        config.fullScale = FullScale::g8;
        config.bandwidth = Device::Bandwidth::hertz50;
        config.enableX = true;
        config.enableY = false;
        config.enableZ = true;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;
        config.blockDataUpdate = false;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0xd8 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x75 }));

        Initialize(config);

        EXPECT_EQ(InitializationResult::success, initializationResult);
    }

    TEST_F(Lis3dshCoreTest, set_power_mode_power_down_writes_ctrl_reg4_with_odr_zero)
    {
        InitializeDefault();

        // powerDown: (0)|0x08|0x07 = 0x0f
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::powerDown, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::powerDown, device.CurrentPowerMode());
    }

    TEST_F(Lis3dshCoreTest, set_power_mode_normal_restores_configured_odr)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        // Restore: (6<<4)|0x08|0x07 = 0x6f
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        infra::VerifyingFunction<void()> done;
        device.SetPowerMode(PowerMode::normal, done);

        ExecuteAllActions();

        EXPECT_EQ(PowerMode::normal, device.CurrentPowerMode());
    }

    TEST_F(Lis3dshCoreTest, set_output_data_rate_writes_ctrl_reg4_with_new_odr)
    {
        InitializeDefault();

        // hertz800=8: (8<<4)|0x08|0x07 = 0x80|0x0f = 0x8f
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x8f }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz800, done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, the_three_slowest_output_data_rates_use_odr_codes_one_to_three)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x1f }));
        device.SetOutputDataRate(OutputDataRate::millihertz3125, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x2f }));
        device.SetOutputDataRate(OutputDataRate::millihertz6250, infra::emptyFunction);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x3f }));
        device.SetOutputDataRate(OutputDataRate::millihertz12500, infra::emptyFunction);
        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, set_output_data_rate_honours_power_down_mode)
    {
        InitializeDefault();

        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));
        device.SetPowerMode(PowerMode::powerDown, infra::emptyFunction);
        ExecuteAllActions();

        // powerDown: odr field = 0, so value = 0x0f regardless of rate
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

        infra::VerifyingFunction<void()> done;
        device.SetOutputDataRate(OutputDataRate::hertz800, done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, set_full_scale_writes_ctrl_reg5_with_new_scale)
    {
        InitializeDefault();

        // g8=3: (0<<6)|(3<<3) = 0x18
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x18 }));

        infra::VerifyingFunction<void()> done;
        device.SetFullScale(FullScale::g8, done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, set_bandwidth_writes_ctrl_reg5_with_new_bandwidth)
    {
        InitializeDefault();

        // hertz50=3, g2=0: (3<<6)|(0<<3) = 0xc0
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0xc0 }));

        infra::VerifyingFunction<void()> done;
        device.SetBandwidth(Device::Bandwidth::hertz50, done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, start_accelerometer_enables_data_ready_via_modify_register_and_uses_rising_edge)
    {
        InitializeDefault();

        // After init, CTRL_REG3 = 0x40 (IEA set, DR_EN clear)
        // EnableDataReadyInterrupt(true): ModifyRegister(0x23, 0x80, 0x80) -> read 0x40, write 0xC0
        ExpectModifyRegister(0x23, 0x40, 0xc0);

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        // risingEdge: dataReadyPin goes high -> triggers callback
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, active_low_polarity_arms_the_pin_with_falling_edge)
    {
        Device::Config config;
        config.interruptPolarity = Device::InterruptPolarity::activeLow;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        Initialize(config);

        EXPECT_CALL(bus, ReadRegisterMock(0x23, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));

        device.AsAccelerometer().Start([](Device::Accelerometer::Samples) {});
        ExecuteAllActions();

        // fallingEdge: must go high first, then low to trigger
        dataReadyPin.SetStubState(true);

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(false);
        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_three_scaled_samples_at_g2)
    {
        InitializeDefault();
        StartStreaming();

        // g2, 61 µg/count: 1000 counts -> 598 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(598, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_three_scaled_samples_at_g4)
    {
        Device::Config config;
        config.fullScale = FullScale::g4;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x40, 0xc0);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        // g4, 122 µg/count: 1000 counts -> 1196 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(1196, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_three_scaled_samples_at_g6)
    {
        Device::Config config;
        config.fullScale = FullScale::g6;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x40, 0xc0);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        // g6, 183 µg/count: 1000 counts -> 1795 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(1795, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_three_scaled_samples_at_g8)
    {
        Device::Config config;
        config.fullScale = FullScale::g8;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x18 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x40, 0xc0);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        // g8, 244 µg/count: 1000 counts -> 2393 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(2393, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_three_scaled_samples_at_g16)
    {
        Device::Config config;
        config.fullScale = FullScale::g16;

        testing::InSequence sequence;
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x20 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        Initialize(config);

        ExpectModifyRegister(0x23, 0x40, 0xc0);
        device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
            {
                for (auto sample : samples)
                    received.push_back(sample.Value());
            });
        ExecuteAllActions();

        // g16, 732 µg/count: 1000 counts -> 7178 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(7178, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_negative_axis_sample_correctly)
    {
        InitializeDefault();
        StartStreaming();

        // g2, 61 µg/count: -1000 counts -> -598 mm/s²
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(-1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(-598, received[0]);
    }

    TEST_F(Lis3dshCoreTest, data_ready_edge_delivers_all_three_axis_samples)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(Measurement(1000, 500, -200)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_EQ(3u, received.size());
        EXPECT_EQ(598, received[0]);
        EXPECT_EQ(299, received[1]);
        EXPECT_EQ(-120, received[2]);
    }

    TEST_F(Lis3dshCoreTest, stop_accelerometer_clears_data_ready_enable)
    {
        InitializeDefault();
        StartStreaming();

        // DisableDataReadyInterrupt: ModifyRegister(0x23, 0x80, 0)
        // After StartStreaming, CTRL_REG3 = 0xC0; clear DR_EN -> 0x40
        ExpectModifyRegister(0x23, 0xc0, 0x40);

        device.AsAccelerometer().Stop();

        ExecuteAllActions();
    }

    TEST_F(Lis3dshCoreTest, stop_completes_via_callback_after_outstanding_transaction)
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

    using PolledDevice = drivers::SensorWithPolling<drivers::Lis3dshCore>;

    class Lis3dshWithPollingTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void InitializeDefault()
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x10 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

            device.Initialize({}, [](InitializationResult) {});
            ForwardTime(std::chrono::milliseconds(15));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x23, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x40 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0xc0 }));

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

    TEST_F(Lis3dshWithPollingTest, poll_reads_status_register_and_delivers_when_data_available)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        ForwardTime(std::chrono::milliseconds(5));
    }

    TEST_F(Lis3dshWithPollingTest, poll_reads_status_but_skips_sample_when_data_not_ready)
    {
        InitializeDefault();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(received.empty());
    }
}
