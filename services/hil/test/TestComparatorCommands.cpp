#include "hal/interfaces/test_doubles/AnalogComparatorMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousAnalogComparatorMock.hpp"
#include "services/hil/commands/ComparatorCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 2> comparatorKeys{ { "neg", "sync" } };

    class ComparatorFactoryStub
        : public services::hil::ComparatorFactory
    {
    public:
        explicit ComparatorFactoryStub(const services::hil::PinNaming& naming)
            : naming(naming)
        {}

        uint8_t Instances() const override
        {
            return 3;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(comparatorKeys);
        }

        services::hil::Status Prepare(uint8_t, const services::hil::Arguments& arguments) override
        {
            services::hil::Status status = services::hil::Status::done;
            arguments.Pin("neg", naming, negative, status);
            arguments.Flag("sync", synchronous, status);

            if (status == services::hil::Status::done && !negative)
                return services::hil::Status::usage;

            return status;
        }

        services::hil::Status Open(uint8_t, const services::hil::Arguments&, services::hil::PinOwner& pins, services::hil::ComparatorHandle& handle) override
        {
            hal::GpioPin* pin = nullptr;
            services::hil::Status status = pins.Claim(*negative, services::hil::PinPool::Use::analog, pin);
            if (status != services::hil::Status::done)
                return status;

            if (synchronous)
                handle.synchronous = &synchronousComparator;
            else
                handle.comparator = &comparator;

            return services::hil::Status::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        const services::hil::PinNaming& naming;
        std::optional<services::hil::PinId> negative;
        bool synchronous = false;
        testing::StrictMock<hal::AnalogComparatorMock> comparator;
        testing::StrictMock<hal::SynchronousAnalogComparatorMock> synchronousComparator;
    };
}

class ComparatorCommandsTest
    : public services::hil::HilFixture
{
public:
    ComparatorFactoryStub factory{ naming };
    services::hil::ComparatorCommands comparator{ context, factory };
};

TEST_F(ComparatorCommandsTest, read_output)
{
    Execute("comp.open 1 neg=PC7");

    EXPECT_CALL(factory.comparator, GetOutput()).WillOnce(testing::Return(true));
    Execute("comp.read 1");

    EXPECT_EQ("OK\r\nOK out=1\r\n", Output());
}

TEST_F(ComparatorCommandsTest, interrupt_counts_output_changes)
{
    Execute("comp.open 0 neg=PC7");
    infra::Function<void(bool output)> onOutputChanged;

    EXPECT_CALL(factory.comparator, Disable());
    EXPECT_CALL(factory.comparator, Enable(testing::_, hal::InterruptTrigger::risingEdge)).WillOnce(testing::SaveArg<0>(&onOutputChanged));
    Execute("comp.irq 0 rising");
    onOutputChanged(true);
    onOutputChanged(false);
    Execute("comp.count 0 clear=1");
    Execute("comp.count 0");

    EXPECT_CALL(factory.comparator, Disable());
    Execute("comp.irq 0 off");

    EXPECT_EQ("OK\r\nOK\r\nOK count=2\r\nOK count=0\r\nOK\r\n", Output());
}

TEST_F(ComparatorCommandsTest, synchronous_comparator_has_no_interrupt)
{
    Execute("comp.open 0 neg=PC7 sync=1");

    EXPECT_CALL(factory.synchronousComparator, GetOutput()).WillOnce(testing::Return(false));
    Execute("comp.read 0");
    Execute("comp.irq 0 both");

    EXPECT_EQ("OK\r\nOK out=0\r\nERR unsupported\r\n", Output());
}

TEST_F(ComparatorCommandsTest, open_and_close_lifecycle)
{
    Execute("comp.open 0");
    Execute("comp.open 3 neg=PC7");
    Execute("comp.open 0 neg=PA0");
    Execute("comp.open 0 neg=PC7");
    Execute("comp.open 1 neg=PC6");
    Execute("comp.close 0");
    Execute("comp.read 0");

    EXPECT_EQ("ERR usage\r\nERR range\r\nERR busy\r\nOK\r\nERR busy\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}
