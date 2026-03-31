#include "drivers/motor_controller/StepperMotorControllerDrv8711.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    StepperMotorControllerDrv8711::StepperMotorControllerDrv8711(hal::SpiMaster& spiWithChipSelect,
        hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
        hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
        hal::GpioPin& resetPin, hal::GpioPin& sleepPin)
        : spi(spiWithChipSelect)
        , faultPin(faultPin)
        , stallBemfPin(stallBemfPin)
        , bemfAnalogPin(bemfAnalogPin)
        , resetPin(resetPin, true)
        , sleepPin(sleepPin, true)
    {}

    void StepperMotorControllerDrv8711::Control(Dtime dtime, Isgain isgain, bool exStall, Mode mode, bool rStep, bool rDir, bool enable, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(dtime) << 10) |
            (static_cast<uint16_t>(isgain) << 8) |
            (static_cast<uint16_t>(exStall) << 7) |
            (static_cast<uint16_t>(mode) << 3) |
            (static_cast<uint16_t>(rStep) << 2) |
            (static_cast<uint16_t>(rDir) << 1) |
            static_cast<uint16_t>(enable);

        WriteRegister(registerControl, data, onDone);
    }

    void StepperMotorControllerDrv8711::Torque(uint8_t torque, Smplth smplth, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(smplth) << 8) |
            static_cast<uint16_t>(torque);

        WriteRegister(registerTorque, data, onDone);
    }

    void StepperMotorControllerDrv8711::Off(uint8_t toff, bool pwmMode, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(pwmMode) << 8) |
            static_cast<uint16_t>(toff);

        WriteRegister(registerOff, data, onDone);
    }

    void StepperMotorControllerDrv8711::Blank(uint8_t tblank, bool abt, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(abt) << 8) |
            static_cast<uint16_t>(tblank);

        WriteRegister(registerBlank, data, onDone);
    }

    void StepperMotorControllerDrv8711::Decay(uint8_t tdecay, DecayMode decayMode, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(decayMode) << 8) |
            static_cast<uint16_t>(tdecay);

        WriteRegister(registerDecay, data, onDone);
    }

    void StepperMotorControllerDrv8711::Stall(uint8_t sdthr, SdCount sdcnt, Vdiv vdiv, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(vdiv) << 10) |
            (static_cast<uint16_t>(sdcnt) << 8) |
            static_cast<uint16_t>(sdthr);

        WriteRegister(registerStall, data, onDone);
    }

    void StepperMotorControllerDrv8711::Drive(OcpThreshold ocpth, OcpDeglitch ocpdeg, DriveTime tdriven, DriveTime tdrivep, IdriveN idriven, IdriveP idrivep, const infra::Function<void()>& onDone)
    {
        uint16_t data = (static_cast<uint16_t>(idrivep) << 10) |
            (static_cast<uint16_t>(idriven) << 8) |
            (static_cast<uint16_t>(tdrivep) << 6) |
            (static_cast<uint16_t>(tdriven) << 4) |
            (static_cast<uint16_t>(ocpdeg) << 2) |
            static_cast<uint16_t>(ocpth);

        WriteRegister(registerDrive, data, onDone);
    }

    void StepperMotorControllerDrv8711::ReadStatus(const infra::Function<void(Status)>& onDone)
    {
        ReadRegister(registerStatus, onDone);
    }

    void StepperMotorControllerDrv8711::OnFault(const infra::Function<void()>& callback)
    {
        faultPin.EnableInterrupt(callback, hal::InterruptTrigger::fallingEdge);
    }

    void StepperMotorControllerDrv8711::OnStall(const infra::Function<void()>& callback)
    {
        stallBemfPin.EnableInterrupt(callback, hal::InterruptTrigger::fallingEdge);
    }

    void StepperMotorControllerDrv8711::OnBemf(const infra::Function<void(MilliVolt voltage)>& callback)
    {
        onBemfCallback = callback;
        bemfAnalogPin.Measure(infra::MakeRange(&bemfSample, &bemfSample + 1), [this]()
            {
                if (onBemfCallback)
                    onBemfCallback(bemfSample);
            });
    }

    void StepperMotorControllerDrv8711::SetReset(bool active)
    {
        resetPin.Set(!active);
    }

    void StepperMotorControllerDrv8711::SetSleep(bool sleep)
    {
        sleepPin.Set(!sleep);
    }

    void StepperMotorControllerDrv8711::WriteRegister(uint8_t address, uint16_t data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onWriteDone);
        this->onWriteDone = onDone;

        uint16_t command = (static_cast<uint16_t>(address & 0x07) << 12) | (data & 0x0FFF);
        sendBuffer[0] = static_cast<uint8_t>(command >> 8);
        sendBuffer[1] = static_cast<uint8_t>(command & 0xFF);

        spi.SendData(infra::MakeByteRange(sendBuffer), hal::SpiAction::stop, [this]()
            {
                this->onWriteDone();
            });
    }

    void StepperMotorControllerDrv8711::ReadRegister(uint8_t address, const infra::Function<void(Status)>& onDone)
    {
        really_assert(!this->onReadDone);
        this->onReadDone = onDone;

        uint16_t command = (1u << 15) | (static_cast<uint16_t>(address & 0x07) << 12);
        sendBuffer[0] = static_cast<uint8_t>(command >> 8);
        sendBuffer[1] = static_cast<uint8_t>(command & 0xFF);

        spi.SendAndReceive(infra::MakeByteRange(sendBuffer), infra::MakeByteRange(receiveBuffer), hal::SpiAction::stop, [this]()
            {
                this->onReadDone(ParseStatus());
            });
    }

    StepperMotorControllerDrv8711::Status StepperMotorControllerDrv8711::ParseStatus()
    {
        uint16_t raw = (static_cast<uint16_t>(receiveBuffer[0]) << 8) | receiveBuffer[1];
        Status status;
        status.overTemperature = (raw & (1u << 0)) != 0;
        status.channelAOverCurrent = (raw & (1u << 1)) != 0;
        status.channelBOverCurrent = (raw & (1u << 2)) != 0;
        status.channelAPreDriverFault = (raw & (1u << 3)) != 0;
        status.channelBPreDriverFault = (raw & (1u << 4)) != 0;
        status.underVoltageLockout = (raw & (1u << 5)) != 0;
        status.stallDetected = (raw & (1u << 6)) != 0;
        status.latchedStallDetect = (raw & (1u << 7)) != 0;
        return status;
    }
}
