#include "drivers/camera/omnivision/OmniVisionSensor.hpp"
#include "hal/interfaces/test_doubles/CameraStub.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using InitResult = drivers::OmniVisionSensor::InitializationResult;

    constexpr hal::CameraFormat testFormat{ 320, 240, hal::CameraPixelFormat::rgb565 };

    constexpr uint8_t productIdReg = 0x0a;
    constexpr uint8_t expectedProductId = 0x76;
    constexpr uint8_t versionReg = 0x0b;
    constexpr std::array<uint8_t, 2> acceptedVersions{ { 0x73, 0x74 } };
    constexpr std::array<drivers::RegisterStep, 1> beforeIdTable{ { drivers::RegisterStep::Write(0x3a, 0x04) } };
    constexpr std::array<drivers::RegisterStep, 1> baseTable{ { drivers::RegisterStep::Write(0x01, 0x10) } };
    constexpr std::array<drivers::RegisterStep, 1> formatTable{ { drivers::RegisterStep::Write(0x02, 0x04) } };
    constexpr std::array<drivers::RegisterStep, 1> resolutionTable{ { drivers::RegisterStep::Write(0x03, 0x3a) } };
    constexpr std::array<drivers::RegisterStep, 1> optionsTable{ { drivers::RegisterStep::Write(0x04, 0x00) } };
    constexpr std::array<drivers::RegisterStep, 1> tuningTable{ { drivers::RegisterStep::Write(0x13, 0xe7) } };

    class TestSensor
        : public drivers::OmniVisionSensor
    {
    public:
        TestSensor(services::RegisterBusAccess& bus, hal::GpioPin& reset,
            hal::GpioPin& powerDown, hal::Camera& capture,
            const Descriptor& desc, hal::CameraFormat format,
            const Timings& timings,
            const infra::Function<void(InitializationResult)>& onInit)
            : OmniVisionSensor(bus, reset, powerDown, capture, desc, format, timings, onInit)
        {}
    };

    class OmniVisionSensorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        OmniVisionSensorTest()
        {
            EXPECT_CALL(capture, Stop()).Times(testing::AnyNumber());
        }

        drivers::OmniVisionSensor::Descriptor MakeDescriptor()
        {
            return {
                productIdReg,
                expectedProductId,
                versionReg,
                infra::MakeRange(acceptedVersions),
                drivers::RegisterStep::Write(0x12, 0x80),
                infra::MakeRange(beforeIdTable),
                infra::MakeRange(baseTable),
                infra::MakeRange(formatTable),
                infra::MakeRange(resolutionTable),
                infra::MakeRange(optionsTable),
                infra::MakeRange(tuningTable)
            };
        }

        void Create(hal::GpioPin& resetPin, hal::GpioPin& powerDownPin)
        {
            sensor.emplace(bus, resetPin, powerDownPin, capture, MakeDescriptor(), testFormat,
                drivers::OmniVisionSensor::Timings{},
                [this](InitResult result)
                {
                    onInitialized.callback(result);
                });
        }

        void Create()
        {
            Create(resetSpy, powerDownSpy);
        }

        void ExpectInitSequence()
        {
            testing::InSequence seq;
            EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
            EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
            EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x73 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x10 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x04 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x03, std::vector<uint8_t>{ 0x3a }));
            EXPECT_CALL(bus, WriteRegisterMock(0x04, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x13, std::vector<uint8_t>{ 0xe7 }));
            EXPECT_CALL(onInitialized, callback(InitResult::success));
        }

        void RunToInitialized()
        {
            ExpectInitSequence();
            Create();
            ForwardTime(std::chrono::milliseconds(22));
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinSpy resetSpy;
        hal::GpioPinSpy powerDownSpy;
        testing::StrictMock<hal::CameraStub> capture;
        std::optional<TestSensor> sensor;
        testing::StrictMock<infra::MockCallback<void(InitResult)>> onInitialized;
    };
}

TEST_F(OmniVisionSensorTest, reset_is_held_low_for_the_reset_pulse_and_then_released)
{
    Create();
    ForwardTime(std::chrono::milliseconds(7));

    ASSERT_FALSE(resetSpy.PinChanges().empty());
    EXPECT_EQ(hal::PinChange(std::chrono::milliseconds(7), true), resetSpy.PinChanges().back());
}

TEST_F(OmniVisionSensorTest, power_down_is_released_before_reset)
{
    Create();
    ForwardTime(std::chrono::milliseconds(7));

    ASSERT_GE(powerDownSpy.PinChanges().size(), 1u);
    ASSERT_GE(resetSpy.PinChanges().size(), 1u);
    EXPECT_LT(powerDownSpy.PinChanges().back().duration, resetSpy.PinChanges().back().duration);
}

TEST_F(OmniVisionSensorTest, without_a_reset_pin_only_a_soft_reset_is_sent)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x73 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x10 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x03, std::vector<uint8_t>{ 0x3a }));
    EXPECT_CALL(bus, WriteRegisterMock(0x04, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x13, std::vector<uint8_t>{ 0xe7 }));
    EXPECT_CALL(onInitialized, callback(InitResult::success));

    Create(hal::dummyPin, hal::dummyPin);
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, soft_reset_is_written_to_the_reset_register_and_waited_for)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));

    Create();
    ForwardTime(std::chrono::milliseconds(12));
}

TEST_F(OmniVisionSensorTest, product_id_and_version_are_read_after_the_reset)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x73 }));
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(onInitialized, callback(testing::_));

    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, a_matching_id_runs_all_tables_in_order)
{
    ExpectInitSequence();
    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, an_unexpected_product_id_stops_with_unexpectedId_and_writes_nothing_more)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x99 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x73 }));
    EXPECT_CALL(onInitialized, callback(InitResult::unexpectedId));

    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, an_unexpected_version_stops_with_unexpectedId)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xff }));
    EXPECT_CALL(onInitialized, callback(InitResult::unexpectedId));

    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, any_accepted_version_is_valid)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x74 }));
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(5);
    EXPECT_CALL(onInitialized, callback(InitResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, onInitialized_is_reported_after_the_last_table_write)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x12, std::vector<uint8_t>{ 0x80 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x76 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x0b, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x73 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x10 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x04 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x03, std::vector<uint8_t>{ 0x3a }));
    EXPECT_CALL(bus, WriteRegisterMock(0x04, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x13, std::vector<uint8_t>{ 0xe7 }));
    EXPECT_CALL(onInitialized, callback(InitResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(22));
}

TEST_F(OmniVisionSensorTest, onInitialized_is_never_called_from_the_constructor)
{
    bool called{ false };
    sensor.emplace(bus, resetSpy, powerDownSpy, capture, MakeDescriptor(), testFormat,
        drivers::OmniVisionSensor::Timings{},
        [&called](InitResult)
        {
            called = true;
        });

    EXPECT_FALSE(called);
}

TEST_F(OmniVisionSensorTest, Start_before_initialization_is_a_programming_error)
{
    std::array<uint8_t, 0> buffer{};
    EXPECT_DEATH(
        {
            sensor.emplace(bus, resetSpy, powerDownSpy, capture, MakeDescriptor(), testFormat,
                drivers::OmniVisionSensor::Timings{},
                [](InitResult) {});
            sensor->Start(testFormat, hal::Camera::Mode::snapshot, buffer, [](hal::Camera::Frame) {}, [](hal::Camera::Error) {});
        },
        "");
}

TEST_F(OmniVisionSensorTest, Start_forwards_format_mode_buffer_and_callbacks_to_the_capture_peripheral)
{
    RunToInitialized();

    std::array<uint8_t, 4> buffer{};
    auto frame = infra::ConstByteRange{};
    auto error = hal::Camera::Error{};

    EXPECT_CALL(capture, Start(testFormat, hal::Camera::Mode::continuous, infra::MakeByteRange(buffer), testing::_, testing::_));

    sensor->Start(testFormat, hal::Camera::Mode::continuous, infra::MakeByteRange(buffer), [&frame](hal::Camera::Frame f)
        {
            frame = f;
        },
        [&error](hal::Camera::Error e)
        {
            error = e;
        });
}

TEST_F(OmniVisionSensorTest, Stop_stops_the_capture_peripheral)
{
    RunToInitialized();

    std::array<uint8_t, 4> buffer{};
    EXPECT_CALL(capture, Start(testing::_, testing::_, testing::_, testing::_, testing::_));
    sensor->Start(testFormat, hal::Camera::Mode::snapshot, infra::MakeByteRange(buffer), [](hal::Camera::Frame) {}, [](hal::Camera::Error) {});

    EXPECT_CALL(capture, Stop());
    sensor->Stop();
}

TEST_F(OmniVisionSensorTest, frames_from_the_peripheral_reach_the_consumer)
{
    RunToInitialized();

    std::array<uint8_t, 4> buffer{ { 0x01, 0x02, 0x03, 0x04 } };
    EXPECT_CALL(capture, Start(testing::_, testing::_, testing::_, testing::_, testing::_));

    infra::ConstByteRange receivedFrame{};
    sensor->Start(testFormat, hal::Camera::Mode::continuous, infra::MakeByteRange(buffer), [&receivedFrame](hal::Camera::Frame f)
        {
            receivedFrame = f;
        },
        [](hal::Camera::Error) {});

    std::array<uint8_t, 2> frameData{ { 0xaa, 0xbb } };
    capture.FrameCaptured(infra::MakeRange(frameData));

    EXPECT_EQ(2u, receivedFrame.size());
}

TEST_F(OmniVisionSensorTest, errors_from_the_peripheral_reach_the_consumer)
{
    RunToInitialized();

    std::array<uint8_t, 4> buffer{};
    EXPECT_CALL(capture, Start(testing::_, testing::_, testing::_, testing::_, testing::_));

    std::optional<hal::Camera::Error> receivedError{};
    sensor->Start(testFormat, hal::Camera::Mode::continuous, infra::MakeByteRange(buffer), [](hal::Camera::Frame) {}, [&receivedError](hal::Camera::Error e)
        {
            receivedError = e;
        });

    capture.Overrun();

    ASSERT_TRUE(receivedError.has_value());
    EXPECT_EQ(hal::Camera::Error::overrun, *receivedError);
}

TEST_F(OmniVisionSensorTest, the_destructor_stops_a_running_capture)
{
    RunToInitialized();

    std::array<uint8_t, 4> buffer{};
    EXPECT_CALL(capture, Start(testing::_, testing::_, testing::_, testing::_, testing::_));
    sensor->Start(testFormat, hal::Camera::Mode::snapshot, infra::MakeByteRange(buffer), [](hal::Camera::Frame) {}, [](hal::Camera::Error) {});

    EXPECT_CALL(capture, Stop());
    sensor.reset();
}
