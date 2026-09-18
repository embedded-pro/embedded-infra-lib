#ifndef DRIVERS_IMU_L3GD20_L3GD20_CORE_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_CORE_HPP

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Gyroscope.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include "infra/util/Unit.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "services/util/RegisterStepRunner.hpp"
#include "services/util/Stoppable.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace drivers
{
    // Drives the L3GD20 and the L3GD20H, which share a register map apart from LOW_ODR.
    // One bus transaction is outstanding at a time, so Initialize, the setters, Start and Stop must
    // not be invoked while a previous one is still running. Completions are delivered from the event
    // dispatcher, so calling them from a completion callback is safe.
    // Call Stop() and destroy only from its callback: an outstanding bus transaction holds a
    // reference, and destroying while referenced trips the assertion in ~AccessedBySharedPtr.
    class L3gd20Core
        : public services::Stoppable
    {
    public:
        using AngularVelocity = infra::Quantity<infra::MilliDegreePerSecond, int32_t>;
        using Temperature = infra::Quantity<infra::MilliCelsius, int32_t>;
        using Gyroscope = hal::Gyroscope<infra::MilliDegreePerSecond, int32_t>;

        enum class Variant : uint8_t
        {
            l3gd20,
            l3gd20h
        };

        // Bit 2 selects the L3GD20H low rate block, bits 1:0 are the CTRL_REG1 data rate field. The
        // L3GD20 runs the same four codes at 95, 190, 380 and 760 Hz, so those names alias the others
        enum class OutputDataRate : uint8_t
        {
            hertz12_5 = 0x04,
            hertz25 = 0x05,
            hertz50 = 0x06,

            hertz100 = 0x00,
            hertz200 = 0x01,
            hertz400 = 0x02,
            hertz800 = 0x03,

            hertz95 = hertz100,
            hertz190 = hertz200,
            hertz380 = hertz400,
            hertz760 = hertz800
        };

        // The cut-off each code selects depends on the output data rate, so the codes are carried
        // through to CTRL_REG1 without being interpreted
        enum class Bandwidth : uint8_t
        {
            cutOff0 = 0,
            cutOff1 = 1,
            cutOff2 = 2,
            cutOff3 = 3
        };

        // The L3GD20H datasheet calls the lowest scale 245 dps; both parts resolve it at
        // 8.75 milli-degrees per second per count, so one set of codes serves both
        enum class FullScale : uint8_t
        {
            dps250 = 0,
            dps500 = 1,
            dps2000 = 2,

            dps245 = dps250
        };

        enum class PowerMode : uint8_t
        {
            normal,
            sleep,
            powerDown
        };

        enum class HighPassMode : uint8_t
        {
            normalWithReset = 0,
            reference = 1,
            normal = 2,
            autoReset = 3
        };

        enum class OutputSelection : uint8_t
        {
            lowPassOnly = 0,
            highPass = 1,
            lowPassTwice = 2
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh = 0,
            activeLow = 1
        };

        enum class InterruptDrive : uint8_t
        {
            pushPull = 0,
            openDrain = 1
        };

        enum class InitializationResult : uint8_t
        {
            success,
            deviceNotFound
        };

        struct Config
        {
            Variant variant = Variant::l3gd20;
            std::optional<uint8_t> expectedIdentification;

            OutputDataRate outputDataRate = OutputDataRate::hertz95;
            Bandwidth bandwidth = Bandwidth::cutOff0;
            FullScale fullScale = FullScale::dps250;
            bool blockDataUpdate = true;
            bool enableX = true;
            bool enableY = true;
            bool enableZ = true;

            HighPassMode highPassMode = HighPassMode::normalWithReset;
            uint8_t highPassCutOff = 0;
            OutputSelection outputSelection = OutputSelection::lowPassOnly;
            uint8_t reference = 0;

            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
            InterruptDrive interruptDrive = InterruptDrive::pushPull;
            InterruptPolarity dataReadyPolarity = InterruptPolarity::activeHigh;

            bool disableI2cInterface = false;
            bool threeWireSpi = false;

            // OUT_TEMP counts down one per degree from a reference the datasheet leaves unspecified,
            // so the zero point is a calibration constant rather than a property of the device
            int32_t temperatureReferenceMilliCelsius = 25000;
            infra::Duration turnOnTime{ std::chrono::milliseconds(250) };
        };

        explicit L3gd20Core(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin, hal::GpioPin& interruptPin = hal::dummyPin);
        L3gd20Core(const L3gd20Core& other) = delete;
        L3gd20Core& operator=(const L3gd20Core& other) = delete;
        ~L3gd20Core();

        // Implementation of services::Stoppable
        void Stop(const infra::Function<void()>& onDone) override;

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);
        bool Initialized() const;

        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetBandwidth(Bandwidth bandwidth, const infra::Function<void()>& onDone);
        void SetFullScale(FullScale scale, const infra::Function<void()>& onDone);
        void SetHighPassFilter(HighPassMode mode, uint8_t cutOff, const infra::Function<void()>& onDone);

        // L3GD20H only; SW_RES clears itself once the reset has run
        void SoftwareReset(const infra::Function<void()>& onDone);

        void MeasureTemperature(const infra::Function<void(Temperature)>& onDone);

        Gyroscope& AsGyroscope();

    protected:
        static constexpr uint8_t registerWhoAmI = 0x0f;
        static constexpr uint8_t registerControl1 = 0x20;
        static constexpr uint8_t registerControl2 = 0x21;
        static constexpr uint8_t registerControl3 = 0x22;
        static constexpr uint8_t registerControl4 = 0x23;
        static constexpr uint8_t registerControl5 = 0x24;
        static constexpr uint8_t registerReference = 0x25;
        static constexpr uint8_t registerOutTemperature = 0x26;
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t registerOutXLow = 0x28;
        static constexpr uint8_t registerFifoControl = 0x2e;
        static constexpr uint8_t registerFifoSource = 0x2f;
        static constexpr uint8_t registerInterrupt1Configuration = 0x30;
        static constexpr uint8_t registerInterrupt1Source = 0x31;
        static constexpr uint8_t registerInterrupt1ThresholdXHigh = 0x32;
        static constexpr uint8_t registerInterrupt1Duration = 0x38;
        static constexpr uint8_t registerLowOutputDataRate = 0x39;

        static constexpr uint8_t identificationL3gd20 = 0xd4;
        static constexpr uint8_t identificationL3gd20h = 0xd7;

        static constexpr uint8_t powerEnable = 0x08;
        static constexpr uint8_t axisEnableZ = 0x04;
        static constexpr uint8_t axisEnableY = 0x02;
        static constexpr uint8_t axisEnableX = 0x01;

        static constexpr uint8_t interrupt1Enable = 0x80;
        static constexpr uint8_t interruptActiveLow = 0x20;
        static constexpr uint8_t openDrain = 0x10;
        static constexpr uint8_t dataReadyInterrupt2 = 0x08;
        static constexpr uint8_t watermarkInterrupt2 = 0x04;
        static constexpr uint8_t overrunInterrupt2 = 0x02;
        static constexpr uint8_t emptyInterrupt2 = 0x01;

        static constexpr uint8_t blockDataUpdateEnable = 0x80;
        static constexpr uint8_t selfTestPositive = 0x02;
        static constexpr uint8_t selfTestNegative = 0x06;
        static constexpr uint8_t serialInterfaceMode3Wire = 0x01;

        static constexpr uint8_t boot = 0x80;
        static constexpr uint8_t fifoEnable = 0x40;
        static constexpr uint8_t stopOnWatermark = 0x20;
        static constexpr uint8_t highPassEnable = 0x10;

        static constexpr uint8_t dataReadyActiveLow = 0x20;
        static constexpr uint8_t i2cDisable = 0x08;
        static constexpr uint8_t softwareResetRequest = 0x04;
        static constexpr uint8_t lowOutputDataRateEnable = 0x01;

        // Contract used by L3gd20WithPolling
        static constexpr uint8_t dataAvailable = 0x08;

        static constexpr uint8_t fifoWatermarkReached = 0x80;
        static constexpr uint8_t fifoOverrun = 0x40;
        static constexpr uint8_t fifoEmpty = 0x20;
        static constexpr uint8_t fifoStoredSamplesMask = 0x1f;

        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;
        static constexpr std::size_t controlRegisterCount = 5;

        static constexpr uint8_t DataRateCode(OutputDataRate rate);
        static constexpr bool IsLowOutputDataRate(OutputDataRate rate);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone);
        void WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone);
        void ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone);

        virtual void StartSampling(const infra::Function<void()>& onSampleAvailable);
        virtual void StopSampling();
        virtual void ReadAndDeliverSamples();
        virtual void ClearCallbacks();

        void EnableDataReadyInterrupt(bool enable);
        void UpdateSampling(bool wanted);
        bool Sampling() const;
        bool TransactionOutstanding() const;
        bool HasLowOutputDataRateRegister() const;
        bool HighPassRequired() const;

        void DeliverAngularVelocity(infra::MemoryRange<const AngularVelocity> samples);

        static int16_t RawSample(const uint8_t* data);
        AngularVelocity ToAngularVelocity(int16_t raw) const;
        Temperature ToTemperature(int8_t raw) const;

        bool GyroscopeRequested() const;
        hal::InterruptTrigger DataReadyTrigger() const;
        hal::InterruptTrigger ThresholdInterruptTrigger() const;

        template<class T>
        infra::SharedPtr<T> KeepAlive(T& object)
        {
            return sharedAccess.MakeShared(object);
        }

        services::RegisterBusAccess& bus;
        hal::InputPin dataReadyPin;
        hal::InputPin interruptPin;
        bool dataReadyPinConnected;
        bool interruptPinConnected;
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        services::RegisterStepRunner runner;
        Config config;
        bool initialized = false;
        bool stopping = false;

    private:
        class GyroscopeAdapter
            : public Gyroscope
        {
        public:
            explicit GyroscopeAdapter(L3gd20Core& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            L3gd20Core& device;
        };

        void PushConfigurationSteps();
        void VerifyIdentification();
        void CompleteInitialization();
        void DeliverMeasurement();
        void DeliverTemperature();
        void ReportStopped();

        uint8_t ExpectedIdentification() const;
        uint8_t Control1Value(PowerMode mode) const;
        uint8_t Control2Value() const;
        uint8_t Control3Value() const;
        uint8_t Control4Value() const;
        uint8_t Control5Value() const;
        uint8_t LowOutputDataRateValue() const;

        GyroscopeAdapter gyroscopeAdapter{ *this };

        infra::Function<void(Gyroscope::Samples)> onGyroscopeMeasurement;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onSequenceDone;
        infra::AutoResetFunction<void(Temperature)> onTemperature;
        infra::AutoResetFunction<void()> onRegisterAccessed;
        infra::AutoResetFunction<void()> onModified;
        infra::AutoResetFunction<void()> onStopped;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<AngularVelocity, axisCount> angularVelocitySamples = {};

        uint8_t scratch = 0;
        uint8_t temperatureBuffer = 0;
        uint8_t writeValue = 0;
        uint8_t modifyValue = 0;
        uint8_t modifyAddress = 0;
        uint8_t modifyClearMask = 0;
        uint8_t modifySetMask = 0;
        PowerMode powerMode = PowerMode::powerDown;
        bool sampling = false;
    };

    ////    Implementation    ////

    constexpr uint8_t L3gd20Core::DataRateCode(OutputDataRate rate)
    {
        return static_cast<uint8_t>(static_cast<uint8_t>(rate) & 0x03);
    }

    constexpr bool L3gd20Core::IsLowOutputDataRate(OutputDataRate rate)
    {
        return (static_cast<uint8_t>(rate) & 0x04) != 0;
    }
}

#endif
