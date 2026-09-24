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
        , resetPin(resetPin, false)
        , sleepPin(sleepPin, true)
    {}

    void StepperMotorControllerDrv8711::Control(Dtime dtime, Isgain isgain, bool exStall, Mode mode, bool rStep, bool rDir, bool enable, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerControl, EncodeControl(dtime, isgain, exStall, mode, rStep, rDir, enable), onDone);
    }

    void StepperMotorControllerDrv8711::Torque(uint8_t torque, Smplth smplth, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerTorque, EncodeTorque(torque, smplth), onDone);
    }

    void StepperMotorControllerDrv8711::Off(uint8_t toff, bool pwmMode, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerOff, EncodeOff(toff, pwmMode), onDone);
    }

    void StepperMotorControllerDrv8711::Blank(uint8_t tblank, bool abt, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerBlank, EncodeBlank(tblank, abt), onDone);
    }

    void StepperMotorControllerDrv8711::Decay(uint8_t tdecay, DecayMode decayMode, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerDecay, EncodeDecay(tdecay, decayMode), onDone);
    }

    void StepperMotorControllerDrv8711::Stall(uint8_t sdthr, SdCount sdcnt, Vdiv vdiv, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerStall, EncodeStall(sdthr, sdcnt, vdiv), onDone);
    }

    void StepperMotorControllerDrv8711::Drive(OcpThreshold ocpth, OcpDeglitch ocpdeg, DriveTime tdriven, DriveTime tdrivep, IdriveN idriven, IdriveP idrivep, const infra::Function<void()>& onDone)
    {
        WriteRegister(registerDrive, EncodeDrive(ocpth, ocpdeg, tdriven, tdrivep, idriven, idrivep), onDone);
    }

    void StepperMotorControllerDrv8711::Configure(const Configuration& configuration, const infra::Function<void(bool verified)>& onDone)
    {
        really_assert(!onConfigured);
        onConfigured = onDone;
        configurationRegisters = EncodeDisabledConfiguration(configuration);
        enableAfterConfiguration = configuration.enable;

        WriteConfigurationRegister(registerControl);
    }

    void StepperMotorControllerDrv8711::ReadStatus(const infra::Function<void(Status)>& onDone)
    {
        really_assert(!onStatusDone);
        onStatusDone = onDone;

        ReadRegister(registerStatus, [this](uint16_t data)
            {
                onStatusDone(ParseStatus(data));
            });
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
        resetPin.Set(active);
    }

    void StepperMotorControllerDrv8711::SetSleep(bool sleep)
    {
        sleepPin.Set(!sleep);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeControl(Dtime dtime, Isgain isgain, bool exStall, Mode mode, bool rStep, bool rDir, bool enable)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(dtime) << 10) |
                                     (static_cast<uint16_t>(isgain) << 8) |
                                     (static_cast<uint16_t>(exStall) << 7) |
                                     (static_cast<uint16_t>(mode) << 3) |
                                     (static_cast<uint16_t>(rStep) << 2) |
                                     (static_cast<uint16_t>(rDir) << 1) |
                                     static_cast<uint16_t>(enable));
    }

    uint16_t StepperMotorControllerDrv8711::EncodeTorque(uint8_t torque, Smplth smplth)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(smplth) << 8) | torque);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeOff(uint8_t toff, bool pwmMode)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(pwmMode) << 8) | toff);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeBlank(uint8_t tblank, bool abt)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(abt) << 8) | tblank);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeDecay(uint8_t tdecay, DecayMode decayMode)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(decayMode) << 8) | tdecay);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeStall(uint8_t sdthr, SdCount sdcnt, Vdiv vdiv)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(vdiv) << 10) |
                                     (static_cast<uint16_t>(sdcnt) << 8) |
                                     sdthr);
    }

    uint16_t StepperMotorControllerDrv8711::EncodeDrive(OcpThreshold ocpth, OcpDeglitch ocpdeg, DriveTime tdriven, DriveTime tdrivep, IdriveN idriven, IdriveP idrivep)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(idrivep) << 10) |
                                     (static_cast<uint16_t>(idriven) << 8) |
                                     (static_cast<uint16_t>(tdrivep) << 6) |
                                     (static_cast<uint16_t>(tdriven) << 4) |
                                     (static_cast<uint16_t>(ocpdeg) << 2) |
                                     static_cast<uint16_t>(ocpth));
    }

    std::array<uint16_t, StepperMotorControllerDrv8711::configurationRegisterCount> StepperMotorControllerDrv8711::EncodeDisabledConfiguration(const Configuration& configuration)
    {
        return { {
            EncodeControl(configuration.dtime, configuration.isgain, configuration.exStall, configuration.mode, false, false, false),
            EncodeTorque(configuration.torque, configuration.smplth),
            EncodeOff(configuration.toff, configuration.pwmMode),
            EncodeBlank(configuration.tblank, configuration.abt),
            EncodeDecay(configuration.tdecay, configuration.decayMode),
            EncodeStall(configuration.sdthr, configuration.sdcnt, configuration.vdiv),
            EncodeDrive(configuration.ocpth, configuration.ocpdeg, configuration.tdriven, configuration.tdrivep, configuration.idriven, configuration.idrivep),
        } };
    }

    uint16_t StepperMotorControllerDrv8711::ComparableReadBack(uint8_t address, uint16_t data)
    {
        const auto comparable = static_cast<uint16_t>(data & 0x0FFF);

        if (address == registerTorque)
            return static_cast<uint16_t>(comparable & ~torqueWriteOnlyBits);

        return comparable;
    }

    void StepperMotorControllerDrv8711::WriteConfigurationRegister(uint8_t address)
    {
        WriteRegister(address, configurationRegisters[address], [this, address]()
            {
                if (address + 1u == configurationRegisterCount)
                    ReadBackConfigurationRegister(registerControl);
                else
                    WriteConfigurationRegister(address + 1);
            });
    }

    void StepperMotorControllerDrv8711::ReadBackConfigurationRegister(uint8_t address)
    {
        ReadRegister(address, [this, address](uint16_t data)
            {
                if (ComparableReadBack(address, data) != ComparableReadBack(address, configurationRegisters[address]))
                    FinishConfiguration(false);
                else if (address + 1u == configurationRegisterCount)
                    ClearStatusAndEnable();
                else
                    ReadBackConfigurationRegister(address + 1);
            });
    }

    void StepperMotorControllerDrv8711::ClearStatusAndEnable()
    {
        WriteRegister(registerStatus, 0, [this]()
            {
                if (!enableAfterConfiguration)
                    FinishConfiguration(true);
                else
                    WriteRegister(registerControl, static_cast<uint16_t>(configurationRegisters[registerControl] | 1u), [this]()
                        {
                            FinishConfiguration(true);
                        });
            });
    }

    void StepperMotorControllerDrv8711::FinishConfiguration(bool verified)
    {
        onConfigured(verified);
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

    void StepperMotorControllerDrv8711::ReadRegister(uint8_t address, const infra::Function<void(uint16_t data)>& onDone)
    {
        really_assert(!this->onReadDone);
        this->onReadDone = onDone;

        uint16_t command = (1u << 15) | (static_cast<uint16_t>(address & 0x07) << 12);
        sendBuffer[0] = static_cast<uint8_t>(command >> 8);
        sendBuffer[1] = static_cast<uint8_t>(command & 0xFF);

        spi.SendAndReceive(infra::MakeByteRange(sendBuffer), infra::MakeByteRange(receiveBuffer), hal::SpiAction::stop, [this]()
            {
                this->onReadDone(static_cast<uint16_t>((static_cast<uint16_t>(receiveBuffer[0]) << 8) | receiveBuffer[1]));
            });
    }

    StepperMotorControllerDrv8711::Status StepperMotorControllerDrv8711::ParseStatus(uint16_t raw)
    {
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
