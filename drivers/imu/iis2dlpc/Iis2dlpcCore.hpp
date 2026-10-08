#ifndef DRIVERS_IMU_IIS2DLPC_IIS2DLPC_CORE_HPP
#define DRIVERS_IMU_IIS2DLPC_IIS2DLPC_CORE_HPP

#include "drivers/imu/common/AccelerometerSensor.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace drivers
{
    class Iis2dlpcCore
        : public AccelerometerSensor
    {
    public:
        // Contract used by SensorWithPolling
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t dataAvailable = 0x01;

        enum class OutputDataRate : uint8_t
        {
            millihertz1600LowPower = 1,
            millihertz12500 = 2,
            hertz25 = 3,
            hertz50 = 4,
            hertz100 = 5,
            hertz200 = 6,
            hertz400 = 7,
            hertz800 = 8,
            hertz1600 = 9
        };

        enum class OperatingMode : uint8_t
        {
            lowPower1,
            lowPower2,
            lowPower3,
            lowPower4,
            highPerformance
        };

        enum class FullScale : uint8_t
        {
            g2 = 0,
            g4 = 1,
            g8 = 2,
            g16 = 3
        };

        enum class FilterBandwidth : uint8_t
        {
            odrDividedBy2 = 0,
            odrDividedBy4 = 1,
            odrDividedBy10 = 2,
            odrDividedBy20 = 3
        };

        enum class PowerMode : uint8_t
        {
            normal,
            powerDown
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh,
            activeLow
        };

        struct Config
        {
            OutputDataRate outputDataRate = OutputDataRate::hertz100;
            OperatingMode operatingMode = OperatingMode::highPerformance;
            FullScale fullScale = FullScale::g2;
            FilterBandwidth filterBandwidth = FilterBandwidth::odrDividedBy2;
            bool lowNoise = false;
            bool blockDataUpdate = true;
            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
        };

        explicit Iis2dlpcCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);

        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetOperatingMode(OperatingMode mode, const infra::Function<void()>& onDone);
        void SetFullScale(FullScale scale, const infra::Function<void()>& onDone);

    protected:
        void ReadAndDeliverSamples() override;
        void EnableDataReadyInterrupt(bool enable) override;
        hal::InterruptTrigger DataReadyTrigger() const override;

    private:
        void VerifyIdentification();
        void CompleteInitialization();
        void DeliverMeasurement();

        uint8_t Control1Value(PowerMode mode) const;
        uint8_t Control2Value() const;
        uint8_t Control3Value() const;
        uint8_t Control6Value() const;
        uint8_t Shift() const;
        int64_t MicroGPerCount() const;

        static bool IsSupported(OutputDataRate rate, OperatingMode mode);

        static constexpr uint8_t registerWhoAmI = 0x0f;
        static constexpr uint8_t registerControl1 = 0x20;
        static constexpr uint8_t registerControl2 = 0x21;
        static constexpr uint8_t registerControl3 = 0x22;
        static constexpr uint8_t registerInterrupt1PadControl = 0x23;
        static constexpr uint8_t registerControl6 = 0x25;
        static constexpr uint8_t registerOutXLow = 0x28;
        static constexpr uint8_t identification = 0x44;

        static constexpr uint8_t highPerformanceMode = 0x04;
        static constexpr uint8_t softReset = 0x40;
        static constexpr uint8_t addressIncrement = 0x04;
        static constexpr uint8_t blockDataUpdateEnable = 0x08;
        static constexpr uint8_t interruptActiveLow = 0x08;
        static constexpr uint8_t dataReadyOnInterrupt1 = 0x01;
        static constexpr uint8_t lowNoiseEnable = 0x04;

        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;
        static constexpr std::chrono::milliseconds resetDelay{ 10 };

        Config config;
        PowerMode powerMode = PowerMode::powerDown;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<Acceleration, axisCount> accelerationSamples = {};

        uint8_t scratch = 0;

        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onSequenceDone;
    };
}

#endif
