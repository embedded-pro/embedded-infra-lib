#ifndef DRIVERS_MOTOR_CONTROLLER_STEPPER_MOTOR_CONTROLLER_DRV8711_HPP
#define DRIVERS_MOTOR_CONTROLLER_STEPPER_MOTOR_CONTROLLER_DRV8711_HPP

#include "hal/interfaces/AnalogToDigitalPin.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Unit.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    using MilliVolt = infra::Quantity<infra::MilliVolt, uint32_t>;

    class StepperMotorControllerDrv8711
    {
    public:
        enum class Dtime : uint8_t
        {
            ns400 = 0,
            ns450 = 1,
            ns650 = 2,
            ns850 = 3
        };

        enum class Isgain : uint8_t
        {
            gain5 = 0,
            gain10 = 1,
            gain20 = 2,
            gain40 = 3
        };

        enum class Mode : uint8_t
        {
            fullStep = 0,
            halfStep = 1,
            step1Over4 = 2,
            step1Over8 = 3,
            step1Over16 = 4,
            step1Over32 = 5,
            step1Over64 = 6,
            step1Over128 = 7,
            step1Over256 = 8
        };

        enum class Smplth : uint8_t
        {
            us50 = 0,
            us100 = 1,
            us200 = 2,
            us300 = 3,
            us400 = 4,
            us600 = 5,
            us800 = 6,
            us1000 = 7
        };

        enum class DecayMode : uint8_t
        {
            slow = 0,
            slowMixed = 1,
            fast = 2,
            mixed = 3,
            slowAutoMixed = 4,
            autoMixed = 5
        };

        enum class Vdiv : uint8_t
        {
            div32 = 0,
            div16 = 1,
            div8 = 2,
            div4 = 3
        };

        enum class SdCount : uint8_t
        {
            sd1 = 0,
            sd2 = 1,
            sd4 = 2,
            sd8 = 3
        };

        enum class OcpThreshold : uint8_t
        {
            mv250 = 0,
            mv500 = 1,
            mv750 = 2,
            mv1000 = 3
        };

        enum class OcpDeglitch : uint8_t
        {
            us1 = 0,
            us2 = 1,
            us4 = 2,
            us8 = 3
        };

        enum class DriveTime : uint8_t
        {
            ns250 = 0,
            ns500 = 1,
            ns1000 = 2,
            ns2000 = 3
        };

        enum class IdriveP : uint8_t
        {
            mA50 = 0,
            mA100 = 1,
            mA150 = 2,
            mA200 = 3
        };

        enum class IdriveN : uint8_t
        {
            mA100 = 0,
            mA200 = 1,
            mA300 = 2,
            mA400 = 3
        };

        struct Status
        {
            bool overTemperature = false;
            bool channelAOverCurrent = false;
            bool channelBOverCurrent = false;
            bool channelAPreDriverFault = false;
            bool channelBPreDriverFault = false;
            bool underVoltageLockout = false;
            bool stallDetected = false;
            bool latchedStallDetect = false;
        };

        struct Configuration
        {
            Dtime dtime{ Dtime::ns850 };
            Isgain isgain{ Isgain::gain5 };
            bool exStall{ false };
            Mode mode{ Mode::fullStep };
            bool enable{ true };
            uint8_t torque{ 0xff };
            Smplth smplth{ Smplth::us100 };
            uint8_t toff{ 0x30 };
            bool pwmMode{ false };
            uint8_t tblank{ 0x80 };
            bool abt{ false };
            uint8_t tdecay{ 0x10 };
            DecayMode decayMode{ DecayMode::slowMixed };
            uint8_t sdthr{ 0x40 };
            SdCount sdcnt{ SdCount::sd1 };
            Vdiv vdiv{ Vdiv::div32 };
            OcpThreshold ocpth{ OcpThreshold::mv500 };
            OcpDeglitch ocpdeg{ OcpDeglitch::us4 };
            DriveTime tdriven{ DriveTime::ns500 };
            DriveTime tdrivep{ DriveTime::ns500 };
            IdriveN idriven{ IdriveN::mA300 };
            IdriveP idrivep{ IdriveP::mA150 };
        };

        StepperMotorControllerDrv8711(hal::SpiMaster& spiWithChipSelect,
            hal::GpioPin& faultPin, hal::GpioPin& stallBemfPin,
            hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin,
            hal::GpioPin& resetPin = hal::dummyPin, hal::GpioPin& sleepPin = hal::dummyPin);

        void Control(Dtime dtime, Isgain isgain, bool exStall, Mode mode, bool rStep, bool rDir, bool enable, const infra::Function<void()>& onDone);
        void Torque(uint8_t torque, Smplth smplth, const infra::Function<void()>& onDone);
        void Off(uint8_t toff, bool pwmMode, const infra::Function<void()>& onDone);
        void Blank(uint8_t tblank, bool abt, const infra::Function<void()>& onDone);
        void Decay(uint8_t tdecay, DecayMode decayMode, const infra::Function<void()>& onDone);
        void Stall(uint8_t sdthr, SdCount sdcnt, Vdiv vdiv, const infra::Function<void()>& onDone);
        void Drive(OcpThreshold ocpth, OcpDeglitch ocpdeg, DriveTime tdriven, DriveTime tdrivep, IdriveN idriven, IdriveP idrivep, const infra::Function<void()>& onDone);

        void Configure(const Configuration& configuration, const infra::Function<void(bool verified)>& onDone);

        void ReadStatus(const infra::Function<void(Status)>& onDone);

        void OnFault(const infra::Function<void()>& callback);
        void OnStall(const infra::Function<void()>& callback);
        void OnBemf(const infra::Function<void(MilliVolt voltage)>& callback);

        void SetReset(bool active);
        void SetSleep(bool sleep);

    private:
        static constexpr uint8_t registerControl = 0x00;
        static constexpr uint8_t registerTorque = 0x01;
        static constexpr uint8_t registerOff = 0x02;
        static constexpr uint8_t registerBlank = 0x03;
        static constexpr uint8_t registerDecay = 0x04;
        static constexpr uint8_t registerStall = 0x05;
        static constexpr uint8_t registerDrive = 0x06;
        static constexpr uint8_t registerStatus = 0x07;
        static constexpr std::size_t configurationRegisterCount = 7;
        static constexpr uint16_t torqueWriteOnlyBits = 1u << 10;

        static uint16_t EncodeControl(Dtime dtime, Isgain isgain, bool exStall, Mode mode, bool rStep, bool rDir, bool enable);
        static uint16_t EncodeTorque(uint8_t torque, Smplth smplth);
        static uint16_t EncodeOff(uint8_t toff, bool pwmMode);
        static uint16_t EncodeBlank(uint8_t tblank, bool abt);
        static uint16_t EncodeDecay(uint8_t tdecay, DecayMode decayMode);
        static uint16_t EncodeStall(uint8_t sdthr, SdCount sdcnt, Vdiv vdiv);
        static uint16_t EncodeDrive(OcpThreshold ocpth, OcpDeglitch ocpdeg, DriveTime tdriven, DriveTime tdrivep, IdriveN idriven, IdriveP idrivep);
        static std::array<uint16_t, configurationRegisterCount> EncodeDisabledConfiguration(const Configuration& configuration);
        static uint16_t ComparableReadBack(uint8_t address, uint16_t data);
        static Status ParseStatus(uint16_t raw);

        void WriteConfigurationRegister(uint8_t address);
        void ReadBackConfigurationRegister(uint8_t address);
        void ClearStatusAndEnable();
        void FinishConfiguration(bool verified);

        void WriteRegister(uint8_t address, uint16_t data, const infra::Function<void()>& onDone);
        void ReadRegister(uint8_t address, const infra::Function<void(uint16_t data)>& onDone);

        hal::SpiMaster& spi;
        hal::InputPin faultPin;
        hal::InputPin stallBemfPin;
        hal::AnalogToDigitalPin<infra::MilliVolt, uint32_t>& bemfAnalogPin;
        hal::OutputPin resetPin;
        hal::OutputPin sleepPin;

        std::array<uint8_t, 2> sendBuffer{};
        std::array<uint8_t, 2> receiveBuffer{};
        infra::AutoResetFunction<void()> onWriteDone;
        infra::AutoResetFunction<void(uint16_t data)> onReadDone;
        infra::AutoResetFunction<void(Status)> onStatusDone;
        infra::AutoResetFunction<void(bool verified)> onConfigured;
        std::array<uint16_t, configurationRegisterCount> configurationRegisters{};
        bool enableAfterConfiguration{ false };
        infra::Function<void(MilliVolt voltage)> onBemfCallback;
        MilliVolt bemfSample{ 0 };
    };
}

#endif
