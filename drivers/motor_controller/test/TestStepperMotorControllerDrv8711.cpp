#include "drivers/motor_controller/DirectPwmStepperMotorDrv8711Decorator.hpp"
#include "drivers/motor_controller/StepDirStepperMotorDrv8711Decorator.hpp"
#include "drivers/motor_controller/StepperMotorControllerDrv8711.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class AnalogToDigitalPinMilliVoltMock
    : public hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>
{
public:
    MOCK_METHOD2(Measure, void(SamplesRange samples, const infra::Function<void()>& onDone));
};

class StepperMotorControllerDrv8711Test
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::SpiMock> spi;
    hal::GpioPinStub faultPinStub;
    hal::GpioPinStub stallBemfPinStub;
    testing::StrictMock<AnalogToDigitalPinMilliVoltMock> bemfAdcMock;
    drivers::StepperMotorControllerDrv8711 driver{ spi, faultPinStub, stallBemfPinStub, bemfAdcMock };
};

class StepperMotorControllerDrv8711WithOptionalPinsTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::SpiMock> spi;
    hal::GpioPinStub faultPinStub;
    hal::GpioPinStub stallBemfPinStub;
    testing::StrictMock<AnalogToDigitalPinMilliVoltMock> bemfAdcMock;
    hal::GpioPinStub resetPinStub;
    hal::GpioPinStub sleepPinStub;
    drivers::StepperMotorControllerDrv8711 driver{ spi, faultPinStub, stallBemfPinStub, bemfAdcMock, resetPinStub, sleepPinStub };
};

TEST_F(StepperMotorControllerDrv8711Test, WriteControlWithEnableOnly)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x00, 0x01 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Control(
        drivers::StepperMotorControllerDrv8711::Dtime::ns400,
        drivers::StepperMotorControllerDrv8711::Isgain::gain5,
        false,
        drivers::StepperMotorControllerDrv8711::Mode::fullStep,
        false, false, true, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteControlAllFieldsSet)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x0F, 0xAF }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Control(
        drivers::StepperMotorControllerDrv8711::Dtime::ns850,
        drivers::StepperMotorControllerDrv8711::Isgain::gain40,
        true,
        drivers::StepperMotorControllerDrv8711::Mode::step1Over32,
        true, true, true, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteTorque)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x12, 0xFF }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Torque(0xFF, drivers::StepperMotorControllerDrv8711::Smplth::us200, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteOff)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x21, 0x30 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Off(0x30, true, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteBlank)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x31, 0x80 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Blank(0x80, true, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteDecay)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x43, 0x10 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Decay(0x10, drivers::StepperMotorControllerDrv8711::DecayMode::mixed, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteStall)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x5A, 0x40 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Stall(0x40,
        drivers::StepperMotorControllerDrv8711::SdCount::sd4,
        drivers::StepperMotorControllerDrv8711::Vdiv::div8, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, WriteDrive)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x69, 0x99 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    driver.Drive(
        drivers::StepperMotorControllerDrv8711::OcpThreshold::mv500,
        drivers::StepperMotorControllerDrv8711::OcpDeglitch::us4,
        drivers::StepperMotorControllerDrv8711::DriveTime::ns500,
        drivers::StepperMotorControllerDrv8711::DriveTime::ns1000,
        drivers::StepperMotorControllerDrv8711::IdriveN::mA200,
        drivers::StepperMotorControllerDrv8711::IdriveP::mA150, done);

    ExecuteAllActions();
}

TEST_F(StepperMotorControllerDrv8711Test, ReadStatusReturnsStatusInCallback)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xF0, 0x00 }, hal::SpiAction::stop));
    EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x25 }));

    drivers::StepperMotorControllerDrv8711::Status receivedStatus;
    driver.ReadStatus([&receivedStatus](drivers::StepperMotorControllerDrv8711::Status status)
        {
            receivedStatus = status;
        });

    ExecuteAllActions();

    EXPECT_TRUE(receivedStatus.overTemperature);
    EXPECT_FALSE(receivedStatus.channelAOverCurrent);
    EXPECT_TRUE(receivedStatus.channelBOverCurrent);
    EXPECT_FALSE(receivedStatus.channelAPreDriverFault);
    EXPECT_FALSE(receivedStatus.channelBPreDriverFault);
    EXPECT_TRUE(receivedStatus.underVoltageLockout);
    EXPECT_FALSE(receivedStatus.stallDetected);
    EXPECT_FALSE(receivedStatus.latchedStallDetect);
}

TEST_F(StepperMotorControllerDrv8711Test, ReadStatusAllFlags)
{
    EXPECT_CALL(spi, SendDataMock(testing::_, hal::SpiAction::stop));
    EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0xFF }));

    drivers::StepperMotorControllerDrv8711::Status receivedStatus;
    driver.ReadStatus([&receivedStatus](drivers::StepperMotorControllerDrv8711::Status status)
        {
            receivedStatus = status;
        });

    ExecuteAllActions();

    EXPECT_TRUE(receivedStatus.overTemperature);
    EXPECT_TRUE(receivedStatus.channelAOverCurrent);
    EXPECT_TRUE(receivedStatus.channelBOverCurrent);
    EXPECT_TRUE(receivedStatus.channelAPreDriverFault);
    EXPECT_TRUE(receivedStatus.channelBPreDriverFault);
    EXPECT_TRUE(receivedStatus.underVoltageLockout);
    EXPECT_TRUE(receivedStatus.stallDetected);
    EXPECT_TRUE(receivedStatus.latchedStallDetect);
}

TEST_F(StepperMotorControllerDrv8711Test, OnFaultRegistersInterrupt)
{
    infra::MockCallback<void()> faultCallback;
    driver.OnFault([&faultCallback]()
        {
            faultCallback.callback();
        });

    faultPinStub.SetStubState(true);
    EXPECT_CALL(faultCallback, callback());
    faultPinStub.SetStubState(false);
}

TEST_F(StepperMotorControllerDrv8711Test, OnStallRegistersInterrupt)
{
    infra::MockCallback<void()> stallCallback;
    driver.OnStall([&stallCallback]()
        {
            stallCallback.callback();
        });

    stallBemfPinStub.SetStubState(true);
    EXPECT_CALL(stallCallback, callback());
    stallBemfPinStub.SetStubState(false);
}

TEST_F(StepperMotorControllerDrv8711Test, OnBemfMeasuresAnalogPin)
{
    EXPECT_CALL(bemfAdcMock, Measure(testing::_, testing::_))
        .WillOnce(testing::Invoke([](hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>::SamplesRange samples, const infra::Function<void()>& onDone)
            {
                samples[0] = drivers::MilliVolt{ 3300 };
                onDone();
            }));

    infra::MockCallback<void(uint32_t)> bemfCallback;
    EXPECT_CALL(bemfCallback, callback(3300));
    driver.OnBemf([&bemfCallback](drivers::MilliVolt voltage)
        {
            bemfCallback.callback(voltage.Value());
        });
}

TEST_F(StepperMotorControllerDrv8711WithOptionalPinsTest, SetResetActiveDrivesLow)
{
    driver.SetReset(true);
    EXPECT_FALSE(resetPinStub.GetStubState());
}

TEST_F(StepperMotorControllerDrv8711WithOptionalPinsTest, SetResetInactiveDrivesHigh)
{
    driver.SetReset(false);
    EXPECT_TRUE(resetPinStub.GetStubState());
}

TEST_F(StepperMotorControllerDrv8711WithOptionalPinsTest, SetSleepTrueDrivesLow)
{
    driver.SetSleep(true);
    EXPECT_FALSE(sleepPinStub.GetStubState());
}

TEST_F(StepperMotorControllerDrv8711WithOptionalPinsTest, SetSleepFalseDrivesHigh)
{
    driver.SetSleep(false);
    EXPECT_TRUE(sleepPinStub.GetStubState());
}

TEST_F(StepperMotorControllerDrv8711Test, ConstructorWithDummyResetAndSleepDoesNotCrash)
{
    testing::StrictMock<hal::SpiMock> spiLocal;
    hal::GpioPinStub faultPin;
    hal::GpioPinStub stallBemfPin;
    testing::StrictMock<AnalogToDigitalPinMilliVoltMock> adcMock;

    drivers::StepperMotorControllerDrv8711 driverLocal(spiLocal, faultPin, stallBemfPin, adcMock);

    driverLocal.SetReset(true);
    driverLocal.SetSleep(true);
}

class StepDirStepperMotorDrv8711DecoratorTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::SpiMock> spi;
    hal::GpioPinStub faultPinStub;
    hal::GpioPinStub stallBemfPinStub;
    testing::StrictMock<AnalogToDigitalPinMilliVoltMock> bemfAdcMock;

    hal::GpioPinStub stepPinStub;
    hal::GpioPinStub dirPinStub;
    drivers::StepDirStepperMotorDrv8711Decorator stepDir{ spi, faultPinStub, stallBemfPinStub, bemfAdcMock, stepPinStub, dirPinStub };
};

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, SetDirectionForward)
{
    stepDir.SetDirection(true);
    EXPECT_TRUE(dirPinStub.GetStubState());
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, SetDirectionReverse)
{
    stepDir.SetDirection(false);
    EXPECT_FALSE(dirPinStub.GetStubState());
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, StepPulsesPin)
{
    stepDir.Step();
    EXPECT_FALSE(stepPinStub.GetStubState());
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, ControlHidesExStallAndRStepAndRDir)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x00, 0x01 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    stepDir.Control(
        drivers::StepperMotorControllerDrv8711::Dtime::ns400,
        drivers::StepperMotorControllerDrv8711::Isgain::gain5,
        drivers::StepperMotorControllerDrv8711::Mode::fullStep,
        true, done);

    ExecuteAllActions();
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, OffHidesPwmMode)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20, 0x30 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    stepDir.Off(0x30, done);

    ExecuteAllActions();
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, TorqueForwards)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x12, 0xFF }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    stepDir.Torque(0xFF, drivers::StepperMotorControllerDrv8711::Smplth::us200, done);

    ExecuteAllActions();
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, DecayForwards)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x43, 0x10 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    stepDir.Decay(0x10, drivers::StepperMotorControllerDrv8711::DecayMode::mixed, done);

    ExecuteAllActions();
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, OnFaultForwards)
{
    infra::MockCallback<void()> faultCallback;
    stepDir.OnFault([&faultCallback]()
        {
            faultCallback.callback();
        });

    faultPinStub.SetStubState(true);
    EXPECT_CALL(faultCallback, callback());
    faultPinStub.SetStubState(false);
}

TEST_F(StepDirStepperMotorDrv8711DecoratorTest, OnStallForwards)
{
    infra::MockCallback<void()> stallCallback;
    stepDir.OnStall([&stallCallback]()
        {
            stallCallback.callback();
        });

    stallBemfPinStub.SetStubState(true);
    EXPECT_CALL(stallCallback, callback());
    stallBemfPinStub.SetStubState(false);
}

class DirectPwmStepperMotorDrv8711DecoratorTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::SpiMock> spi;
    hal::GpioPinStub faultPinStub;
    hal::GpioPinStub stallBemfPinStub;
    testing::StrictMock<AnalogToDigitalPinMilliVoltMock> bemfAdcMock;

    drivers::DirectPwmStepperMotorDrv8711Decorator directPwm{ spi, faultPinStub, stallBemfPinStub, bemfAdcMock };
};

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, ControlHidesRStepAndRDir)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x00, 0x81 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    directPwm.Control(
        drivers::StepperMotorControllerDrv8711::Dtime::ns400,
        drivers::StepperMotorControllerDrv8711::Isgain::gain5,
        true,
        drivers::StepperMotorControllerDrv8711::Mode::fullStep,
        true, done);

    ExecuteAllActions();
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, OffForwardsWithPwmMode)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x21, 0x30 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    directPwm.Off(0x30, true, done);

    ExecuteAllActions();
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, BlankForwards)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x31, 0x80 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    directPwm.Blank(0x80, true, done);

    ExecuteAllActions();
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, StallForwards)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x5A, 0x40 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    directPwm.Stall(0x40,
        drivers::StepperMotorControllerDrv8711::SdCount::sd4,
        drivers::StepperMotorControllerDrv8711::Vdiv::div8, done);

    ExecuteAllActions();
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, DriveForwards)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x69, 0x99 }, hal::SpiAction::stop));

    infra::VerifyingFunction<void()> done;
    directPwm.Drive(
        drivers::StepperMotorControllerDrv8711::OcpThreshold::mv500,
        drivers::StepperMotorControllerDrv8711::OcpDeglitch::us4,
        drivers::StepperMotorControllerDrv8711::DriveTime::ns500,
        drivers::StepperMotorControllerDrv8711::DriveTime::ns1000,
        drivers::StepperMotorControllerDrv8711::IdriveN::mA200,
        drivers::StepperMotorControllerDrv8711::IdriveP::mA150, done);

    ExecuteAllActions();
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, OnBemfForwardsWithVoltage)
{
    EXPECT_CALL(bemfAdcMock, Measure(testing::_, testing::_))
        .WillOnce(testing::Invoke([](hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>::SamplesRange samples, const infra::Function<void()>& onDone)
            {
                samples[0] = drivers::MilliVolt{ 1500 };
                onDone();
            }));

    infra::MockCallback<void(uint32_t)> bemfCallback;
    EXPECT_CALL(bemfCallback, callback(1500));
    directPwm.OnBemf([&bemfCallback](drivers::MilliVolt voltage)
        {
            bemfCallback.callback(voltage.Value());
        });
}

TEST_F(DirectPwmStepperMotorDrv8711DecoratorTest, OnFaultForwards)
{
    infra::MockCallback<void()> faultCallback;
    directPwm.OnFault([&faultCallback]()
        {
            faultCallback.callback();
        });

    faultPinStub.SetStubState(true);
    EXPECT_CALL(faultCallback, callback());
    faultPinStub.SetStubState(false);
}
