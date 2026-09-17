#include "drivers/imu/mpu9250/Mpu9250WithSelfTest.hpp"
#include "drivers/imu/mpu9250/Mpu9250WithWakeOnMotion.hpp"
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
    using SelfTestDevice = drivers::Mpu9250WithSelfTest<drivers::Mpu9250Core>;
    using WakeOnMotionDevice = drivers::Mpu9250WithWakeOnMotion<drivers::Mpu9250Core>;
    using SelfTestResult = SelfTestDevice::SelfTestResult;

    template<class Device>
    class Mpu9250Fixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Mpu9250Fixture()
        {
            EXPECT_CALL(bus, RequiresI2cSlaveInterfaceDisabled()).WillRepeatedly(testing::Return(false));
        }

        void Initialize()
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

            this->ForwardTime(std::chrono::milliseconds(101));
        }

        static std::vector<uint8_t> AllAxes(int16_t value)
        {
            std::vector<uint8_t> data;

            for (int index = 0; index != 7; ++index)
            {
                int16_t word = index == 3 ? 0 : value;
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(word) >> 8));
                data.push_back(static_cast<uint8_t>(word & 0xff));
            }

            return data;
        }

        testing::StrictMock<drivers::Mpu9250BusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
    };

    class Mpu9250WithSelfTestTest
        : public Mpu9250Fixture<SelfTestDevice>
    {
    public:
        void RunSelfTest(int16_t response, uint8_t trim)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x19, 5)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x04, 0x03, 0x00, 0x00, 0x03 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x00, 0x02, 0x00, 0x00, 0x02 }));

            EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14))
                .Times(2 * SelfTestDevice::sampleCount)
                .WillRepeatedly(testing::Invoke([this, response](uint8_t, std::size_t)
                    {
                        return AllAxes(sampleReads++ < SelfTestDevice::sampleCount ? 0 : response);
                    }));

            EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0xe0 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0xe0 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x00 }));

            EXPECT_CALL(bus, ReadRegisterMock(0x0d, 3)).WillOnce(testing::Return(std::vector<uint8_t>{ trim, trim, trim }));
            EXPECT_CALL(bus, ReadRegisterMock(0x00, 3)).WillOnce(testing::Return(std::vector<uint8_t>{ trim, trim, trim }));

            EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x04, 0x03, 0x00, 0x00, 0x03 }));

            device.SelfTest([this](SelfTestResult value)
                {
                    result = value;
                });

            ForwardTime(std::chrono::milliseconds(41));
        }

        uint32_t sampleReads = 0;
        std::optional<SelfTestResult> result;
    };

    class Mpu9250WithWakeOnMotionTest
        : public Mpu9250Fixture<WakeOnMotionDevice>
    {};
}

TEST(Mpu9250SelfTestOtpTest, factory_trim_of_one_is_the_base_value)
{
    EXPECT_EQ(2620u, SelfTestDevice::FactoryTrimResponse(1));
}

TEST(Mpu9250SelfTestOtpTest, factory_trim_grows_by_one_percent_per_step)
{
    EXPECT_EQ(2646u, SelfTestDevice::FactoryTrimResponse(2));
    EXPECT_EQ(32804u, SelfTestDevice::FactoryTrimResponse(255));
}

TEST(Mpu9250SelfTestOtpTest, an_unprogrammed_factory_trim_yields_zero)
{
    EXPECT_EQ(0u, SelfTestDevice::FactoryTrimResponse(0));
}

TEST(Mpu9250SelfTestOtpTest, the_acceptance_window_spans_half_to_one_and_a_half_times_the_factory_trim)
{
    EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(2620, 2620));
    EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(1310, 2620));
    EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(3930, 2620));

    EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(1309, 2620));
    EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(3931, 2620));
}

TEST(Mpu9250SelfTestOtpTest, a_negative_response_is_judged_on_its_magnitude)
{
    EXPECT_TRUE(SelfTestDevice::WithinAcceptanceWindow(-2620, 2620));
}

TEST(Mpu9250SelfTestOtpTest, an_unprogrammed_factory_trim_never_passes)
{
    EXPECT_FALSE(SelfTestDevice::WithinAcceptanceWindow(2620, 0));
}

TEST_F(Mpu9250WithSelfTestTest, self_test_saves_forces_and_restores_the_datasheet_configuration)
{
    Initialize();
    RunSelfTest(2620, 1);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result->Passed());
}

TEST_F(Mpu9250WithSelfTestTest, self_test_fails_when_the_response_is_below_the_acceptance_window)
{
    Initialize();
    RunSelfTest(1000, 1);

    ASSERT_TRUE(result);
    EXPECT_FALSE(result->Passed());
    EXPECT_FALSE(result->accelerometerX);
    EXPECT_FALSE(result->gyroscopeZ);
}

TEST_F(Mpu9250WithSelfTestTest, self_test_fails_when_the_response_is_above_the_acceptance_window)
{
    Initialize();
    RunSelfTest(8000, 1);

    ASSERT_TRUE(result);
    EXPECT_FALSE(result->Passed());
}

TEST_F(Mpu9250WithSelfTestTest, self_test_does_not_complete_before_the_settling_delays_have_elapsed)
{
    Initialize();

    EXPECT_CALL(bus, ReadRegisterMock(0x19, 5)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x04, 0x03, 0x00, 0x00, 0x03 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x00, 0x02, 0x00, 0x00, 0x02 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14))
        .Times(SelfTestDevice::sampleCount)
        .WillRepeatedly(testing::Return(AllAxes(0)));
    EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0xe0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0xe0 }));

    device.SelfTest([this](SelfTestResult value)
        {
            result = value;
        });

    ForwardTime(std::chrono::milliseconds(19));

    EXPECT_FALSE(result);
}

TEST_F(Mpu9250WithWakeOnMotionTest, enable_configures_low_power_cycle_mode_and_the_threshold)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x69, std::vector<uint8_t>{ 0xc0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1f, std::vector<uint8_t>{ 0x14 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1e, std::vector<uint8_t>{ 0x06 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x6b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x21 }));

    infra::VerifyingFunction<void()> done;
    device.EnableWakeOnMotion(80, WakeOnMotionDevice::LowPowerOutputDataRate::milliHertz15630, []() {}, done);

    ExecuteAllActions();
}

TEST_F(Mpu9250WithWakeOnMotionTest, the_threshold_is_clamped_to_the_register_range)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x69, std::vector<uint8_t>{ 0xc0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1f, std::vector<uint8_t>{ 0xff }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1e, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x6b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x21 }));

    infra::VerifyingFunction<void()> done;
    device.EnableWakeOnMotion(5000, WakeOnMotionDevice::LowPowerOutputDataRate::milliHertz240, []() {}, done);

    ExecuteAllActions();
}

TEST_F(Mpu9250WithWakeOnMotionTest, a_motion_interrupt_clears_the_status_and_reports_the_motion)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x69, std::vector<uint8_t>{ 0xc0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1f, std::vector<uint8_t>{ 0x14 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1e, std::vector<uint8_t>{ 0x06 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x6b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x21 }));

    infra::MockCallback<void()> motion;
    infra::VerifyingFunction<void()> done;
    device.EnableWakeOnMotion(80, WakeOnMotionDevice::LowPowerOutputDataRate::milliHertz15630, [&motion]()
        {
            motion.callback();
        },
        done);

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(motion, callback());

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
}

TEST_F(Mpu9250WithWakeOnMotionTest, disable_restores_the_normal_power_configuration)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x69, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x03 }));

    infra::VerifyingFunction<void()> done;
    device.DisableWakeOnMotion(done);

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
}

TEST_F(Mpu9250WithWakeOnMotionTest, an_interrupt_without_the_wake_on_motion_bit_reports_nothing)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x07 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x69, std::vector<uint8_t>{ 0xc0 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1f, std::vector<uint8_t>{ 0x14 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x1e, std::vector<uint8_t>{ 0x06 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x6b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x21 }));

    testing::StrictMock<infra::MockCallback<void()>> motion;
    infra::VerifyingFunction<void()> done;
    device.EnableWakeOnMotion(80, WakeOnMotionDevice::LowPowerOutputDataRate::milliHertz15630, [&motion]()
        {
            motion.callback();
        },
        done);

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();
}
