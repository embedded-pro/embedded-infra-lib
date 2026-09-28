#include "hal/interfaces/test_doubles/WatchdogMock.hpp"
#include "services/hil/commands/WatchDogCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 3> watchDogKeys{ { "timeout", "feed", "reset" } };

    class WatchDogFactoryStub
        : public services::hil::WatchDogFactory
    {
    public:
        uint8_t Instances() const override
        {
            return 2;
        }

        infra::MemoryRange<const char* const> StartKeys() const override
        {
            return infra::MakeRange(watchDogKeys);
        }

        services::hil::Status Prepare(uint8_t, const services::hil::Arguments& arguments) override
        {
            services::hil::Status status = services::hil::Status::done;
            arguments.Flag("reset", reset, status);
            return status;
        }

        services::hil::Status Create(uint8_t, infra::Duration timeout, const services::hil::Arguments&, hal::Watchdog*& watchDog) override
        {
            this->timeout = timeout;
            watchDog = &this->watchDog;
            return services::hil::Status::done;
        }

        bool reset = true;
        infra::Duration timeout{};
        testing::StrictMock<hal::WatchdogMock> watchDog;
    };
}

class WatchDogCommandsTest
    : public services::hil::HilFixture
{
public:
    WatchDogFactoryStub factory;
    services::hil::WatchDogCommands watchDog{ context, factory };
    infra::Function<void()> onEarlyWarning;
};

TEST_F(WatchDogCommandsTest, start_and_feed)
{
    EXPECT_CALL(factory.watchDog, Start(testing::_));
    Execute("wdt.start 1 timeout=500 reset=0 feed=manual");
    EXPECT_EQ(std::chrono::milliseconds(500), factory.timeout);
    EXPECT_FALSE(factory.reset);

    EXPECT_CALL(factory.watchDog, Refresh());
    Execute("wdt.feed 1");
    Execute("wdt.feed 0");
    Execute("wdt.start 0 timeout=500");

    EXPECT_EQ("OK\r\nOK\r\nERR notopen\r\nERR busy\r\n", Output());
}

TEST_F(WatchDogCommandsTest, start_checks_its_arguments)
{
    Execute("wdt.start 0");
    Execute("wdt.start 2 timeout=500");
    Execute("wdt.start 0 timeout=30001");
    Execute("wdt.start 0 timeout=500 reset=2 feed=sometimes");
    Execute("wdt.start 0 timeout=500 feed=sometimes");
    Execute("wdt.start 0 timeout=500 speed=1");

    EXPECT_EQ("ERR usage\r\nERR range\r\nERR range\r\nERR range\r\nERR usage\r\nERR usage\r\n", Output());
}

TEST_F(WatchDogCommandsTest, early_warning_feeds_automatically_and_reports)
{
    EXPECT_CALL(factory.watchDog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&onEarlyWarning));
    Execute("wdt.start 0 timeout=100");
    Output();

    onEarlyWarning();
    onEarlyWarning();
    EXPECT_CALL(factory.watchDog, Refresh());
    ExecuteAllActions();

    EXPECT_EQ("\r\nEVT wdt index=0 warning=2\r\n", Output());
}

TEST_F(WatchDogCommandsTest, manual_feed_only_reports_early_warning)
{
    EXPECT_CALL(factory.watchDog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&onEarlyWarning));
    Execute("wdt.start 0 timeout=100 feed=manual");
    Output();

    onEarlyWarning();
    ExecuteAllActions();

    EXPECT_EQ("\r\nEVT wdt index=0 warning=1\r\n", Output());
}
