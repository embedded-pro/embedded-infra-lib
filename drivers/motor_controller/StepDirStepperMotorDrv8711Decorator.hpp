#ifndef DRIVERS_MOTOR_CONTROLLER_STEP_DIR_STEPPER_MOTOR_DRV8711_DECORATOR_HPP
#define DRIVERS_MOTOR_CONTROLLER_STEP_DIR_STEPPER_MOTOR_DRV8711_DECORATOR_HPP

#include "drivers/motor_controller/StepperMotorControllerDrv8711.hpp"

namespace drivers
{
    class StepDirStepperMotorDrv8711Decorator
    {
    public:
        StepDirStepperMotorDrv8711Decorator(hal::SpiMaster& spi,
            hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
            hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
            hal::GpioPin& stepPin, hal::GpioPin& dirPin,
            hal::GpioPin& resetPin = hal::dummyPin, hal::GpioPin& sleepPin = hal::dummyPin);

        void SetDirection(bool forward);
        void Step();

        void Control(StepperMotorControllerDrv8711::Dtime dtime, StepperMotorControllerDrv8711::Isgain isgain,
            StepperMotorControllerDrv8711::Mode mode, bool enable, const infra::Function<void()>& onDone);
        void Torque(uint8_t torque, StepperMotorControllerDrv8711::Smplth smplth, const infra::Function<void()>& onDone);
        void Off(uint8_t toff, const infra::Function<void()>& onDone);
        void Decay(uint8_t tdecay, StepperMotorControllerDrv8711::DecayMode decayMode, const infra::Function<void()>& onDone);

        void ReadStatus(const infra::Function<void(StepperMotorControllerDrv8711::Status)>& onDone);

        void OnFault(const infra::Function<void()>& callback);
        void OnStall(const infra::Function<void()>& callback);

        void SetReset(bool active);
        void SetSleep(bool sleep);

    private:
        StepperMotorControllerDrv8711 controller;
        hal::OutputPin stepPin;
        hal::OutputPin dirPin;
    };
}

#endif
