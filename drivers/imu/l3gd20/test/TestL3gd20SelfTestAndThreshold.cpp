#include "drivers/imu/l3gd20/L3gd20WithSelfTest.hpp"
#include "drivers/imu/l3gd20/L3gd20WithThresholdInterrupt.hpp"
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
    using Core = drivers::L3gd20Core;
    using Variant = Core::Variant;

    using SelfTestDevice = drivers::L3gd20WithSelfTest<Core>;
    using ThresholdDevice = drivers::L3gd20WithThresholdInterrupt<Core>;

    template<class Device>
    class L3gd20DeviceFixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Initialize(Variant variant = Variant::l3gd20)
        {
            Core::Config config;
            config.variant = variant;
            config.turnOnTime = std::chrono::milliseconds(5);

            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ variant == Variant::l3gd20h ? uint8_t{ 0xd7 } : uint8_t{ 0xd4 } }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));

            if (variant == Variant::l3gd20h)
                EXPECT_CALL(bus, WriteRegisterMock(0x39, std::vector<uint8_t>{ 0x00 }));

            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

            this->device.Initialize(config, [](Core::InitializationResult) {});
            this->ForwardTime(std::chrono::milliseconds(30));
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
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
        hal::GpioPinStub interruptPin;
        Device device{ bus, dataReadyPin, interruptPin };
    };

    class L3gd20SelfTestTest
        : public L3gd20DeviceFixture<SelfTestDevice>
    {
    public:
        // The procedure takes one reading it throws away before averaging, on each side of the test
        static constexpr std::size_t readsPerAverage = SelfTestDevice::sampleCount + 1;

        void ExpectSelfTestSequence(std::array<int16_t, 3> without, std::array<int16_t, 3> with, uint8_t savedControl1 = 0x0f)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x20, 5)).WillOnce(testing::Return(std::vector<uint8_t>{ savedControl1, 0x00, 0x00, 0x80, 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f, 0x00, 0x00, 0xa0, 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0xa2 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0xa0 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ savedControl1, 0x00, 0x00, 0x80, 0x00 }));

            EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillRepeatedly(testing::Invoke([this, without, with](uint8_t, std::size_t)
                {
                    const auto& source = readIndex++ < readsPerAverage ? without : with;

                    return Measurement(source[0], source[1], source[2]);
                }));
        }

        void RunSelfTest()
        {
            device.SelfTest([this](SelfTestDevice::SelfTestResult measured)
                {
                    result = measured;
                });

            ForwardTime(std::chrono::milliseconds(1000));
        }

        std::size_t readIndex = 0;
        std::optional<SelfTestDevice::SelfTestResult> result;
    };

    class L3gd20ThresholdTest
        : public L3gd20DeviceFixture<ThresholdDevice>
    {
    public:
        void EnableThreshold(const ThresholdDevice::ThresholdConfig& thresholdConfig, const std::vector<uint8_t>& thresholdBytes, uint8_t configuration, uint8_t duration = 0x00, uint8_t control5 = 0x00)
        {
            // Ordered, because the line may only be armed once the thresholds behind it are in place
            testing::InSequence sequence;

            EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x32, thresholdBytes));
            EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ duration }));
            EXPECT_CALL(bus, ReadRegisterMock(0x24, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ control5 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ configuration }));
            EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x80 }));

            infra::VerifyingFunction<void()> done;
            device.EnableThresholdInterrupt(thresholdConfig, [this](ThresholdDevice::ThresholdEvent measured)
                {
                    event = measured;
                },
                done);

            ExecuteAllActions();
        }

        std::optional<ThresholdDevice::ThresholdEvent> event;
    };

    TEST(L3gd20SelfTestWindowTest, the_acceptance_window_spans_the_datasheet_minimum_and_maximum)
    {
        EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(2500, 2500, 12500));
        EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(12500, 2500, 12500));
        EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(2499, 2500, 12500));
        EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(12501, 2500, 12500));
    }

    // The sign of the response differs between the two parts and between the two test polarities
    TEST(L3gd20SelfTestWindowTest, a_negative_response_is_judged_on_its_magnitude)
    {
        EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(-5000, 2500, 12500));
        EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(-1000, 2500, 12500));
    }

    TEST(L3gd20SelfTestWindowTest, an_absent_response_never_passes)
    {
        EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(0, 2500, 12500));
    }

    TEST_F(L3gd20SelfTestTest, self_test_saves_forces_and_restores_the_control_registers)
    {
        Initialize();
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 5000, 5000, 5000 } }, 0x4f);
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_TRUE(result->Passed());
    }

    TEST_F(L3gd20SelfTestTest, self_test_passes_when_every_axis_responds_within_the_window)
    {
        Initialize();
        ExpectSelfTestSequence({ { 10, -20, 30 } }, { { 5010, 4980, -4970 } });
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_TRUE(result->Passed());
        EXPECT_EQ((std::array<int32_t, 3>{ { 5000, 5000, -5000 } }), result->delta);
    }

    TEST_F(L3gd20SelfTestTest, self_test_fails_when_the_response_is_below_the_window)
    {
        Initialize();
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 1000, 1000, 1000 } });
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_FALSE(result->Passed());
    }

    TEST_F(L3gd20SelfTestTest, self_test_fails_when_the_response_is_above_the_window)
    {
        Initialize();
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 20000, 20000, 20000 } });
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_FALSE(result->Passed());
    }

    TEST_F(L3gd20SelfTestTest, self_test_fails_only_the_axis_that_is_out_of_range)
    {
        Initialize();
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 5000, 100, 5000 } });
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_TRUE(result->x);
        EXPECT_FALSE(result->y);
        EXPECT_TRUE(result->z);
        EXPECT_EQ(100, result->delta[1]);
    }

    TEST_F(L3gd20SelfTestTest, the_acceptance_window_is_configurable)
    {
        Initialize();
        device.SetAcceptanceWindow(50, 200);
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 100, 100, 100 } });
        RunSelfTest();

        ASSERT_TRUE(result);
        EXPECT_TRUE(result->Passed());
    }

    TEST_F(L3gd20SelfTestTest, self_test_does_not_complete_before_the_settling_times_have_elapsed)
    {
        Initialize();
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 5000, 5000, 5000 } });

        device.SelfTest([this](SelfTestDevice::SelfTestResult measured)
            {
                result = measured;
            });

        ForwardTime(std::chrono::milliseconds(900));
        EXPECT_FALSE(result);

        ForwardTime(std::chrono::milliseconds(100));
        EXPECT_TRUE(result);
    }

    TEST_F(L3gd20SelfTestTest, the_settling_times_are_configurable)
    {
        Initialize();
        device.SetSettlingTimes(std::chrono::milliseconds(10), std::chrono::milliseconds(2));
        ExpectSelfTestSequence({ { 0, 0, 0 } }, { { 5000, 5000, 5000 } });

        device.SelfTest([this](SelfTestDevice::SelfTestResult measured)
            {
                result = measured;
            });

        ForwardTime(std::chrono::milliseconds(20));

        EXPECT_TRUE(result);
    }

    TEST_F(L3gd20ThresholdTest, enabling_writes_the_thresholds_high_byte_first)
    {
        Initialize();

        ThresholdDevice::ThresholdConfig thresholdConfig;
        thresholdConfig.threshold = { { 0x0102, 0x0304, 0x0506 } };

        // High byte at the lower address, the opposite of the output registers
        EnableThreshold(thresholdConfig, std::vector<uint8_t>{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, 0x6a);
    }

    TEST_F(L3gd20ThresholdTest, enabling_selects_the_configured_axes_and_duration)
    {
        Initialize();

        ThresholdDevice::ThresholdConfig thresholdConfig;
        thresholdConfig.highY = false;
        thresholdConfig.lowX = true;
        thresholdConfig.combineWithAnd = true;
        thresholdConfig.latch = false;
        thresholdConfig.duration = 7;
        thresholdConfig.waitBeforeRelease = true;

        EnableThreshold(thresholdConfig, std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0xa3, 0x87);
    }

    TEST_F(L3gd20ThresholdTest, the_high_pass_filter_is_selectable_as_the_interrupt_source)
    {
        Initialize();

        ThresholdDevice::ThresholdConfig thresholdConfig;
        thresholdConfig.useHighPassFilter = true;

        EnableThreshold(thresholdConfig, std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6a, 0x00, 0x14);
    }

    TEST_F(L3gd20ThresholdTest, the_decrement_mode_bit_rides_in_the_top_bit_of_the_first_threshold)
    {
        Initialize(Variant::l3gd20h);

        ThresholdDevice::ThresholdConfig thresholdConfig;
        thresholdConfig.threshold = { { 0x0102, 0, 0 } };
        thresholdConfig.decrementMode = true;

        EnableThreshold(thresholdConfig, std::vector<uint8_t>{ 0x81, 0x02, 0x00, 0x00, 0x00, 0x00 }, 0x6a);
    }

    TEST_F(L3gd20ThresholdTest, an_edge_reads_the_source_register_and_reports_the_axis)
    {
        Initialize();
        EnableThreshold(ThresholdDevice::ThresholdConfig(), std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6a);

        EXPECT_CALL(bus, ReadRegisterMock(0x31, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x62 }));

        interruptPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_TRUE(event);
        EXPECT_TRUE(event->highX);
        EXPECT_FALSE(event->lowX);
        EXPECT_FALSE(event->highY);
        EXPECT_TRUE(event->highZ);
    }

    TEST_F(L3gd20ThresholdTest, an_edge_without_the_active_bit_reports_nothing)
    {
        Initialize();
        EnableThreshold(ThresholdDevice::ThresholdConfig(), std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6a);

        EXPECT_CALL(bus, ReadRegisterMock(0x31, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        interruptPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_FALSE(event);
    }

    TEST_F(L3gd20ThresholdTest, disabling_disarms_the_line_and_clears_the_configuration)
    {
        Initialize();
        EnableThreshold(ThresholdDevice::ThresholdConfig(), std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6a);

        ExpectModifyRegister(0x22, 0x80, 0x00);
        EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));

        infra::VerifyingFunction<void()> done;
        device.DisableThresholdInterrupt(done);
        ExecuteAllActions();

        // The pin is disarmed, so a further edge reaches neither the bus nor the callback
        interruptPin.SetStubState(false);
        interruptPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_FALSE(event);
    }

    TEST_F(L3gd20ThresholdTest, stopping_clears_a_registered_threshold_callback)
    {
        Initialize();
        EnableThreshold(ThresholdDevice::ThresholdConfig(), std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x6a);

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);
        ExecuteAllActions();

        interruptPin.SetStubState(false);
        interruptPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_FALSE(event);
    }
}
