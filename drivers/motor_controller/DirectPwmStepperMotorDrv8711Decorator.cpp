#include "drivers/motor_controller/DirectPwmStepperMotorDrv8711Decorator.hpp"

namespace drivers
{
    DirectPwmStepperMotorDrv8711Decorator::DirectPwmStepperMotorDrv8711Decorator(hal::SpiMaster& spi,
        hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
        hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
        hal::GpioPin& resetPin, hal::GpioPin& sleepPin)
        : controller(spi, faultPin, stallBemfPin, bemfAnalogPin, resetPin, sleepPin)
    {}

    void DirectPwmStepperMotorDrv8711Decorator::Control(StepperMotorControllerDrv8711::Dtime dtime, StepperMotorControllerDrv8711::Isgain isgain,
        bool exStall, StepperMotorControllerDrv8711::Mode mode, bool enable, const infra::Function<void()>& onDone)
    {
        controller.Control(dtime, isgain, exStall, mode, false, false, enable, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Torque(uint8_t torque, StepperMotorControllerDrv8711::Smplth smplth, const infra::Function<void()>& onDone)
    {
        controller.Torque(torque, smplth, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Off(uint8_t toff, bool pwmMode, const infra::Function<void()>& onDone)
    {
        controller.Off(toff, pwmMode, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Blank(uint8_t tblank, bool abt, const infra::Function<void()>& onDone)
    {
        controller.Blank(tblank, abt, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Decay(uint8_t tdecay, StepperMotorControllerDrv8711::DecayMode decayMode, const infra::Function<void()>& onDone)
    {
        controller.Decay(tdecay, decayMode, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Stall(uint8_t sdthr, StepperMotorControllerDrv8711::SdCount sdcnt,
        StepperMotorControllerDrv8711::Vdiv vdiv, const infra::Function<void()>& onDone)
    {
        controller.Stall(sdthr, sdcnt, vdiv, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::Drive(StepperMotorControllerDrv8711::OcpThreshold ocpth, StepperMotorControllerDrv8711::OcpDeglitch ocpdeg,
        StepperMotorControllerDrv8711::DriveTime tdriven, StepperMotorControllerDrv8711::DriveTime tdrivep,
        StepperMotorControllerDrv8711::IdriveN idriven, StepperMotorControllerDrv8711::IdriveP idrivep,
        const infra::Function<void()>& onDone)
    {
        controller.Drive(ocpth, ocpdeg, tdriven, tdrivep, idriven, idrivep, onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::ReadStatus(const infra::Function<void(StepperMotorControllerDrv8711::Status)>& onDone)
    {
        controller.ReadStatus(onDone);
    }

    void DirectPwmStepperMotorDrv8711Decorator::OnFault(const infra::Function<void()>& callback)
    {
        controller.OnFault(callback);
    }

    void DirectPwmStepperMotorDrv8711Decorator::OnStall(const infra::Function<void()>& callback)
    {
        controller.OnStall(callback);
    }

    void DirectPwmStepperMotorDrv8711Decorator::OnBemf(const infra::Function<void(MilliVolt voltage)>& callback)
    {
        controller.OnBemf(callback);
    }

    void DirectPwmStepperMotorDrv8711Decorator::SetReset(bool active)
    {
        controller.SetReset(active);
    }

    void DirectPwmStepperMotorDrv8711Decorator::SetSleep(bool sleep)
    {
        controller.SetSleep(sleep);
    }
}
