#include "hal/interfaces/test_doubles/AdcMultiChannelMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousAdcMock.hpp"
#include "services/hil/commands/AdcCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 2> adcKeys{ { "pins", "sync" } };

    class AdcFactoryStub
        : public services::hil::AdcFactory
    {
    public:
        explicit AdcFactoryStub(const services::hil::PinNaming& naming)
            : naming(naming)
        {}

        std::size_t KeyPositionals() const override
        {
            return 2;
        }

        services::hil::Status ParseKey(const services::hil::Arguments& arguments, uint16_t& key) const override
        {
            uint32_t adc = 0;
            uint32_t sequencer = 0;
            services::hil::Status status = services::hil::Status::done;
            arguments.NumberAt(0, adc, 0, 1, status);
            arguments.NumberAt(1, sequencer, 0, 3, status);
            key = static_cast<uint16_t>(adc * 4 + sequencer);
            return status;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(adcKeys);
        }

        services::hil::Status Prepare(uint16_t, const services::hil::Arguments& arguments) override
        {
            services::hil::Status status = services::hil::Status::done;
            arguments.Pin("pins", naming, pin, status);
            arguments.Flag("sync", synchronous, status);
            return status;
        }

        services::hil::Status Open(uint16_t key, const services::hil::Arguments&, services::hil::PinOwner& pins, services::hil::AdcHandle& handle) override
        {
            hal::GpioPin* claimed = nullptr;
            services::hil::Status status = pins.ClaimAnalog(pin.value_or(services::hil::PinId{ 4, 0 }), claimed);
            if (status != services::hil::Status::done)
                return status;

            owners[key] = pins.Id();
            handle.samplesPerRun = 2;
            if (synchronous)
                handle.synchronous = &synchronousAdc;
            else
                handle.adc = &adc;

            return services::hil::Status::done;
        }

        void Close(uint16_t key, const infra::Function<void()>& onClosed) override
        {
            closed[key] = true;
            onClosed();
        }

        const services::hil::PinNaming& naming;
        std::optional<services::hil::PinId> pin;
        bool synchronous = false;
        std::array<services::hil::Owner, 8> owners{};
        std::array<bool, 8> closed{};
        testing::StrictMock<hal::AdcMultiChannelMock> adc;
        testing::StrictMock<hal::SynchronousAdcMock> synchronousAdc;
    };
}

class AdcCommandsTest
    : public services::hil::HilFixture
{
public:
    void Open(const char* line)
    {
        Execute(line);
        ASSERT_EQ("OK\r\n", Output());
    }

    AdcFactoryStub factory{ naming };
    services::hil::AdcCommands::WithCapacity<2, 4> adc{ context, factory };
    infra::Function<void(hal::AdcMultiChannel::Samples)> onSamples;
    std::array<uint16_t, 2> samples{ { 100, 4095 } };
};

TEST_F(AdcCommandsTest, open_uses_one_owner_per_slot)
{
    Open("adc.open 0 0");
    Open("adc.open 1 0");

    EXPECT_EQ(services::hil::owner::adc, factory.owners[0]);
    EXPECT_EQ(services::hil::owner::adc + 1, factory.owners[4]);
    EXPECT_EQ((services::hil::PinId{ 4, 0 }), pinFactory.constructed[0]);
}

TEST_F(AdcCommandsTest, open_reports_errors)
{
    Open("adc.open 0 0");

    Execute("adc.open 0 0");
    Execute("adc.open 0 4");
    Execute("adc.open 0");
    Execute("adc.open 0 1 pins=PB0");
    Execute("adc.open 0 1 pins=PZ0");
    EXPECT_EQ("ERR busy\r\nERR range\r\nERR usage\r\nERR pin\r\nERR pin\r\n", Output());

    Open("adc.open 0 1");
    Execute("adc.open 0 2");
    EXPECT_EQ("ERR busy\r\n", Output());
}

TEST_F(AdcCommandsTest, synchronous_measure)
{
    factory.synchronous = true;
    Open("adc.open 0 0");

    EXPECT_CALL(factory.synchronousAdc, Measure(2)).Times(2).WillRepeatedly(testing::Return(infra::MakeRange(std::as_const(samples))));
    Execute("adc.measure 0 0 n=2");
    Execute("adc.measure 0 0 n=3");

    EXPECT_EQ("OK samples=100,4095,100,4095\r\nERR range\r\n", Output());
}

TEST_F(AdcCommandsTest, asynchronous_measure_collects_runs)
{
    Open("adc.open 0 0");

    EXPECT_CALL(factory.adc, Measure(testing::_)).WillOnce(testing::SaveArg<0>(&onSamples));
    Execute("adc.measure 0 0 n=2");
    Execute("adc.measure 0 0");
    EXPECT_EQ("ERR busy\r\n", Output());

    onSamples(infra::MakeRange(std::as_const(samples)));
    EXPECT_CALL(factory.adc, Stop());
    onSamples(infra::MakeRange(std::as_const(samples)));
    ExecuteAllActions();

    EXPECT_EQ("\r\nOK samples=100,4095,100,4095\r\n", Output());
}

TEST_F(AdcCommandsTest, asynchronous_measure_times_out)
{
    Open("adc.open 0 0");

    EXPECT_CALL(factory.adc, Measure(testing::_));
    Execute("adc.measure 0 0");
    EXPECT_CALL(factory.adc, Stop());
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ("\r\nERR timeout\r\n", Output());
}

TEST_F(AdcCommandsTest, close_while_measuring_fails_the_measurement)
{
    Open("adc.open 0 0");
    EXPECT_CALL(factory.adc, Measure(testing::_));
    Execute("adc.measure 0 0");

    EXPECT_CALL(factory.adc, Stop());
    Execute("adc.close 0 0");
    Execute("adc.measure 0 0");

    EXPECT_EQ("ERR failed\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_TRUE(factory.closed[0]);
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}

TEST_F(AdcCommandsTest, shared_analog_pin_stays_claimed_until_last_close)
{
    Open("adc.open 0 0");
    Open("adc.open 1 0");

    Execute("adc.close 0 0");
    EXPECT_TRUE(pinFactory.constructed[0].has_value());

    Execute("adc.close 1 0");
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
    EXPECT_EQ("OK\r\nOK\r\n", Output());
}
