#include "hal/interfaces/test_doubles/PwmMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousPwmMock.hpp"
#include "services/hil/HilCommand.hpp"
#include "gmock/gmock.h"
#include <variant>

namespace
{
    using PwmDriver = std::variant<std::monostate, testing::StrictMock<hal::PwmMock>, testing::StrictMock<hal::SynchronousPwmMock>>;
}

TEST(HilWithDriverTest, does_nothing_without_a_driver)
{
    PwmDriver driver;
    uint32_t calls = 0;

    services::HilWithDriver(driver, [&calls]<class Driver>(Driver&)
        {
            ++calls;
        });

    EXPECT_EQ(0, calls);
}

TEST(HilWithDriverTest, invokes_the_action_on_the_active_driver)
{
    PwmDriver driver;
    const auto start = [](hal::DutyCycle dutyCycle)
    {
        return [dutyCycle]<class Driver>(Driver& pwm)
        {
            pwm.Start(dutyCycle);
        };
    };

    auto& synchronous = driver.emplace<testing::StrictMock<hal::SynchronousPwmMock>>();
    EXPECT_CALL(synchronous, Start(hal::DutyCycle::FromPercent(25)));
    services::HilWithDriver(driver, start(hal::DutyCycle::FromPercent(25)));

    auto& asynchronous = driver.emplace<testing::StrictMock<hal::PwmMock>>();
    EXPECT_CALL(asynchronous, Start(hal::DutyCycle::FromPercent(75)));
    services::HilWithDriver(driver, start(hal::DutyCycle::FromPercent(75)));
}
