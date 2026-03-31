#ifndef DRIVERS_MOTOR_CONTROLLER_DIRECT_PWM_STEPPER_MOTOR_DRV8711_DECORATOR_HPP
#define DRIVERS_MOTOR_CONTROLLER_DIRECT_PWM_STEPPER_MOTOR_DRV8711_DECORATOR_HPP

#include "drivers/motor_controller/StepperMotorControllerDrv8711.hpp"

namespace drivers
{
    class DirectPwmStepperMotorDrv8711Decorator
    {
    public:
        DirectPwmStepperMotorDrv8711Decorator(hal::SpiMaster& spi,
            hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
            hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
            hal::GpioPin& resetPin = hal::dummyPin, hal::GpioPin& sleepPin = hal::dummyPin);

        void Control(StepperMotorControllerDrv8711::Dtime dtime, StepperMotorControllerDrv8711::Isgain isgain,
            bool exStall, StepperMotorControllerDrv8711::Mode mode, bool enable, const infra::Function<void()>& onDone);
        void Torque(uint8_t torque, StepperMotorControllerDrv8711::Smplth smplth, const infra::Function<void()>& onDone);
        void Off(uint8_t toff, bool pwmMode, const infra::Function<void()>& onDone);
        void Blank(uint8_t tblank, bool abt, const infra::Function<void()>& onDone);
        void Decay(uint8_t tdecay, StepperMotorControllerDrv8711::DecayMode decayMode, const infra::Function<void()>& onDone);
        void Stall(uint8_t sdthr, StepperMotorControllerDrv8711::SdCount sdcnt,
            StepperMotorControllerDrv8711::Vdiv vdiv, const infra::Function<void()>& onDone);
        void Drive(StepperMotorControllerDrv8711::OcpThreshold ocpth, StepperMotorControllerDrv8711::OcpDeglitch ocpdeg,
            StepperMotorControllerDrv8711::DriveTime tdriven, StepperMotorControllerDrv8711::DriveTime tdrivep,
            StepperMotorControllerDrv8711::IdriveN idriven, StepperMotorControllerDrv8711::IdriveP idrivep,
            const infra::Function<void()>& onDone);

        void ReadStatus(const infra::Function<void(StepperMotorControllerDrv8711::Status)>& onDone);

        void OnFault(const infra::Function<void()>& callback);
        void OnStall(const infra::Function<void()>& callback);
        void OnBemf(const infra::Function<void(MilliVolt voltage)>& callback);

        void SetReset(bool active);
        void SetSleep(bool sleep);

    private:
        StepperMotorControllerDrv8711 controller;
    };
}

#endif
