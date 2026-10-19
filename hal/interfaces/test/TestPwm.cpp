#include "hal/interfaces/Pwm.hpp"
#include "gmock/gmock.h"
#include <type_traits>

namespace
{
    class FullPwmMock
        : public hal::SingleChannelPwm
        , public hal::TwoChannelsPwm
        , public hal::ThreeChannelsPwm
        , public hal::FourChannelsPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (hal::Hertz baseFrequency), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, Start, (hal::DutyCycle globalDutyCycle), (override));
        MOCK_METHOD(void, Start, (hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2), (override));
        MOCK_METHOD(void, Start, (hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3), (override));
        MOCK_METHOD(void, Start, (hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3, hal::DutyCycle dutyCycle4), (override));
    };
}

TEST(PwmTest, a_driver_implementing_every_channel_count_converts_to_the_common_pwm)
{
    static_assert(std::is_convertible_v<FullPwmMock&, hal::Pwm&>);
}

TEST(PwmTest, stop_through_the_common_pwm_reaches_the_driver)
{
    testing::StrictMock<FullPwmMock> driver;
    hal::Pwm& pwm = driver;

    EXPECT_CALL(driver, Stop());
    pwm.Stop();
}

TEST(PwmTest, set_base_frequency_through_the_common_pwm_reaches_the_driver)
{
    testing::StrictMock<FullPwmMock> driver;
    hal::Pwm& pwm = driver;

    EXPECT_CALL(driver, SetBaseFrequency(hal::Hertz{ 20000 }));
    pwm.SetBaseFrequency(hal::Hertz{ 20000 });
}

TEST(PwmTest, the_common_pwm_is_the_same_object_for_every_channel_count_interface)
{
    testing::StrictMock<FullPwmMock> driver;
    hal::SingleChannelPwm& single = driver;
    hal::TwoChannelsPwm& two = driver;
    hal::ThreeChannelsPwm& three = driver;
    hal::FourChannelsPwm& four = driver;
    hal::Pwm& pwm = driver;

    EXPECT_EQ(static_cast<hal::Pwm*>(&single), &pwm);
    EXPECT_EQ(static_cast<hal::Pwm*>(&two), &pwm);
    EXPECT_EQ(static_cast<hal::Pwm*>(&three), &pwm);
    EXPECT_EQ(static_cast<hal::Pwm*>(&four), &pwm);
}

TEST(PwmTest, start_through_each_channel_count_interface_reaches_the_matching_overload)
{
    testing::StrictMock<FullPwmMock> driver;
    hal::SingleChannelPwm& single = driver;
    hal::TwoChannelsPwm& two = driver;
    hal::ThreeChannelsPwm& three = driver;
    hal::FourChannelsPwm& four = driver;
    const auto duty = hal::DutyCycle::FromPercent(25);

    EXPECT_CALL(driver, Start(duty));
    EXPECT_CALL(driver, Start(duty, duty));
    EXPECT_CALL(driver, Start(duty, duty, duty));
    EXPECT_CALL(driver, Start(duty, duty, duty, duty));

    single.Start(duty);
    two.Start(duty, duty);
    three.Start(duty, duty, duty);
    four.Start(duty, duty, duty, duty);
}
