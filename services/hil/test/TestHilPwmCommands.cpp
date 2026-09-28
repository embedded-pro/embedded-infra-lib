#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "services/hil/commands/HilPwmCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

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

    class ThreeChannelsSynchronousPwmMock
        : public hal::SynchronousThreeChannelsPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (hal::Hertz baseFrequency), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, Start, (hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3), (override));
    };

    constexpr std::array<const char*, 1> pwmKeys{ { "sync" } };

    class PwmFactoryStub
        : public services::HilPwmFactory
    {
    public:
        uint8_t Instances() const override
        {
            return 2;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(pwmKeys);
        }

        services::HilStatus Prepare(uint8_t, const services::HilArguments& arguments) override
        {
            services::HilStatus status = services::HilStatus::done;
            arguments.Flag("sync", synchronous, status);
            return status;
        }

        services::HilStatus Open(uint8_t moduleIndex, const services::HilArguments&, services::HilPinOwner& pins, services::HilPwmHandle*& handle) override
        {
            hal::GpioPin* pin = nullptr;
            services::HilStatus status = pins.ClaimFunction(services::HilPinId{ 1, 6 }, 10, moduleIndex, pin);
            if (status != services::HilStatus::done)
                return status;

            if (synchronous)
                handle = &synchronousAdapter;
            else
                handle = &adapter;

            return services::HilStatus::done;
        }

        void ReportOpened(uint8_t, services::HilResponse::Line& line) override
        {
            line << " pwmclk=" << 40000000u;
        }

        services::HilStatus ChangeFrequency(uint8_t, uint32_t hertz) override
        {
            if (hertz > 100000)
                return services::HilStatus::range;

            return services::HilStatus::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        bool synchronous = false;
        testing::StrictMock<FullPwmMock> pwm;
        testing::StrictMock<ThreeChannelsSynchronousPwmMock> synchronousPwm;
        services::HilPwmAdapter<FullPwmMock> adapter{ pwm, 2 };
        services::HilPwmAdapter<ThreeChannelsSynchronousPwmMock> synchronousAdapter{ synchronousPwm, 3 };
    };

    const hal::DutyCycle half{ hal::DutyCycle::fullScale / 2 };
    const hal::DutyCycle full{ hal::DutyCycle::fullScale };
}

class HilPwmCommandsTest
    : public services::HilFixture
{
public:
    PwmFactoryStub factory;
    services::HilPwmCommands pwm{ context, factory };
};

TEST_F(HilPwmCommandsTest, open_reports_factory_details)
{
    Execute("pwm.open 1");
    Execute("pwm.open 0");
    Execute("pwm.open 2");
    Execute("pwm.open 0 sync=3");

    EXPECT_EQ("OK pwmclk=40000000\r\nERR busy\r\nERR range\r\nERR range\r\n", Output());
    EXPECT_EQ((services::HilPinId{ 1, 6 }), pinFactory.constructed[0]);
}

TEST_F(HilPwmCommandsTest, duty_accepts_one_or_all_channels)
{
    Execute("pwm.open 0");

    EXPECT_CALL(factory.pwm, Start(half));
    Execute("pwm.duty 0 50");
    EXPECT_CALL(factory.pwm, Start(half, full));
    Execute("pwm.duty 0 50 100");
    Execute("pwm.duty 0 50 50 50");
    Execute("pwm.duty 0 101");
    Execute("pwm.duty 1 50");

    EXPECT_EQ("OK pwmclk=40000000\r\nOK\r\nOK\r\nERR usage\r\nERR usage\r\nERR notopen\r\n", Output());
}

TEST_F(HilPwmCommandsTest, single_duty_is_replicated_for_drivers_without_global_start)
{
    Execute("pwm.open 0 sync=1");

    EXPECT_CALL(factory.synchronousPwm, Start(half, half, half));
    Execute("pwm.duty 0 50");
    EXPECT_CALL(factory.synchronousPwm, Start(half, full, half));
    Execute("pwm.duty 0 50 100 50");

    EXPECT_EQ("OK pwmclk=40000000\r\nOK\r\nOK\r\n", Output());
}

TEST_F(HilPwmCommandsTest, frequency_is_validated_by_factory)
{
    Execute("pwm.open 0");

    EXPECT_CALL(factory.pwm, SetBaseFrequency(hal::Hertz(20000)));
    Execute("pwm.freq 0 20000");
    Execute("pwm.freq 0 200000");
    Execute("pwm.freq 0 0");

    EXPECT_EQ("OK pwmclk=40000000\r\nOK\r\nERR range\r\nERR range\r\n", Output());
}

TEST_F(HilPwmCommandsTest, stop_and_close)
{
    Execute("pwm.open 0");

    EXPECT_CALL(factory.pwm, Stop());
    Execute("pwm.stop 0");
    Execute("pwm.close 0");
    Execute("pwm.stop 0");

    EXPECT_EQ("OK pwmclk=40000000\r\nOK\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}
