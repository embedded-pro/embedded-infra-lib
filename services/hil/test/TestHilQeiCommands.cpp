#include "hal/synchronous_interfaces/test_doubles/SynchronousQuadratureEncoderMock.hpp"
#include "services/hil/commands/HilQeiCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 1> qeiKeys{ { "res" } };

    class QeiFactoryStub
        : public services::HilQeiFactory
    {
    public:
        uint8_t Instances() const override
        {
            return 2;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(qeiKeys);
        }

        services::HilStatus Prepare(uint8_t, const services::HilArguments& arguments) override
        {
            uint32_t resolution = 1024;
            services::HilStatus status = services::HilStatus::done;
            arguments.Number("res", resolution, 1, 4096, status);
            return status;
        }

        services::HilStatus Open(uint8_t index, const services::HilArguments&, services::HilPinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) override
        {
            hal::GpioPin* pin = nullptr;
            services::HilStatus status = pins.ClaimFunction(services::HilPinId{ 3, 6 }, 20, index, pin);
            if (status != services::HilStatus::done)
                return status;

            encoder = &this->encoder;
            return services::HilStatus::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        testing::StrictMock<hal::SynchronousQuadratureEncoderMock> encoder;
    };
}

class HilQeiCommandsTest
    : public services::HilFixture
{
public:
    QeiFactoryStub factory;
    services::HilQeiCommands qei{ context, factory };
};

TEST_F(HilQeiCommandsTest, read_reports_position_direction_speed_and_resolution)
{
    Execute("qei.open 0 res=2048");

    EXPECT_CALL(factory.encoder, Position()).WillOnce(testing::Return(12));
    EXPECT_CALL(factory.encoder, Direction()).WillOnce(testing::Return(hal::SynchronousQuadratureEncoder::MotionDirection::reverse));
    EXPECT_CALL(factory.encoder, Speed()).WillOnce(testing::Return(300));
    EXPECT_CALL(factory.encoder, Resolution()).WillOnce(testing::Return(2048));
    Execute("qei.read 0");

    EXPECT_EQ("OK\r\nOK pos=12 dir=rev speed=300 res=2048\r\n", Output());
}

TEST_F(HilQeiCommandsTest, open_and_close_lifecycle)
{
    Execute("qei.open 0 res=0");
    Execute("qei.read 0");
    Execute("qei.open 0");
    Execute("qei.open 1");
    Execute("qei.close 1");
    Execute("qei.close 0");
    Execute("qei.close 0");

    EXPECT_EQ("ERR range\r\nERR notopen\r\nOK\r\nERR busy\r\nERR notopen\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}
