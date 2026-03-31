#include "drivers/motor_controller/StepDirStepperMotorDrv8711Decorator.hpp"

namespace drivers
{
    StepDirStepperMotorDrv8711Decorator::StepDirStepperMotorDrv8711Decorator(hal::SpiMaster& spi,
        hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
        hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
        hal::GpioPin& stepPin, hal::GpioPin& dirPin,
        hal::GpioPin& resetPin, hal::GpioPin& sleepPin)
        : controller(spi, faultPin, stallBemfPin, bemfAnalogPin, resetPin, sleepPin)
        , stepPin(stepPin)
        , dirPin(dirPin)
    {}

    void StepDirStepperMotorDrv8711Decorator::SetDirection(bool forward)
    {
        dirPin.Set(forward);
    }

    void StepDirStepperMotorDrv8711Decorator::Step()
    {
        stepPin.Set(true);
        stepPin.Set(false);
    }

    void StepDirStepperMotorDrv8711Decorator::Control(StepperMotorControllerDrv8711::Dtime dtime, StepperMotorControllerDrv8711::Isgain isgain,
        StepperMotorControllerDrv8711::Mode mode, bool enable, const infra::Function<void()>& onDone)
    {
        controller.Control(dtime, isgain, false, mode, false, false, enable, onDone);
    }

    void StepDirStepperMotorDrv8711Decorator::Torque(uint8_t torque, StepperMotorControllerDrv8711::Smplth smplth, const infra::Function<void()>& onDone)
    {
        controller.Torque(torque, smplth, onDone);
    }

    void StepDirStepperMotorDrv8711Decorator::Off(uint8_t toff, const infra::Function<void()>& onDone)
    {
        controller.Off(toff, false, onDone);
    }

    void StepDirStepperMotorDrv8711Decorator::Decay(uint8_t tdecay, StepperMotorControllerDrv8711::DecayMode decayMode, const infra::Function<void()>& onDone)
    {
        controller.Decay(tdecay, decayMode, onDone);
    }

    void StepDirStepperMotorDrv8711Decorator::ReadStatus(const infra::Function<void(StepperMotorControllerDrv8711::Status)>& onDone)
    {
        controller.ReadStatus(onDone);
    }

    void StepDirStepperMotorDrv8711Decorator::OnFault(const infra::Function<void()>& callback)
    {
        controller.OnFault(callback);
    }

    void StepDirStepperMotorDrv8711Decorator::OnStall(const infra::Function<void()>& callback)
    {
        controller.OnStall(callback);
    }

    void StepDirStepperMotorDrv8711Decorator::SetReset(bool active)
    {
        controller.SetReset(active);
    }

    void StepDirStepperMotorDrv8711Decorator::SetSleep(bool sleep)
    {
        controller.SetSleep(sleep);
    }
}
