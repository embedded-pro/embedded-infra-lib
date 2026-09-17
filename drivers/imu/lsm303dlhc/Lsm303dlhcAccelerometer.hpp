#ifndef DRIVERS_IMU_LSM303DLHC_LSM303DLHC_ACCELEROMETER_HPP
#define DRIVERS_IMU_LSM303DLHC_LSM303DLHC_ACCELEROMETER_HPP

#include "drivers/imu/lsm303dlhc/Lsm303dlhcSensor.hpp"
#include "hal/interfaces/Accelerometer.hpp"
#include "infra/util/Unit.hpp"
#include <array>

namespace drivers
{
    // The linear acceleration block has no identification register, so Initialize verifies presence
    // by reading back the control register it has just written
    class Lsm303dlhcAccelerometer
        : public Lsm303dlhcSensor
    {
    public:
        using Acceleration = infra::Quantity<infra::MilliMeterPerSecondSquared, int32_t>;
        using Accelerometer = hal::Accelerometer<infra::MilliMeterPerSecondSquared, int32_t>;

        enum class OutputDataRate : uint8_t
        {
            powerDown = 0,
            hertz1 = 1,
            hertz10 = 2,
            hertz25 = 3,
            hertz50 = 4,
            hertz100 = 5,
            hertz200 = 6,
            hertz400 = 7,
            hertz1620LowPower = 8,
            hertz1344 = 9
        };

        enum class FullScale : uint8_t
        {
            g2 = 0,
            g4 = 1,
            g8 = 2,
            g16 = 3
        };

        enum class PowerMode : uint8_t
        {
            normal,
            lowPower,
            powerDown
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh = 0,
            activeLow = 1
        };

        struct Config
        {
            OutputDataRate outputDataRate = OutputDataRate::hertz100;
            FullScale fullScale = FullScale::g2;
            bool highResolution = true;
            bool blockDataUpdate = true;
            bool enableX = true;
            bool enableY = true;
            bool enableZ = true;
            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
            bool latchInterrupt = false;
        };

        explicit Lsm303dlhcAccelerometer(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);
        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetFullScale(FullScale scale, const infra::Function<void()>& onDone);
        void SetHighResolution(bool highResolution, const infra::Function<void()>& onDone);

        Accelerometer& AsAccelerometer();

    protected:
        static constexpr uint8_t registerControl1 = 0x20;
        static constexpr uint8_t registerControl2 = 0x21;
        static constexpr uint8_t registerControl3 = 0x22;
        static constexpr uint8_t registerControl4 = 0x23;
        static constexpr uint8_t registerControl5 = 0x24;
        static constexpr uint8_t registerControl6 = 0x25;
        static constexpr uint8_t registerReference = 0x26;
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t registerOutXLow = 0x28;
        static constexpr uint8_t registerFifoControl = 0x2e;
        static constexpr uint8_t registerFifoSource = 0x2f;

        static constexpr uint8_t boot = 0x80;
        static constexpr uint8_t lowPowerEnable = 0x08;
        static constexpr uint8_t blockDataUpdateEnable = 0x80;
        static constexpr uint8_t highResolutionEnable = 0x08;
        static constexpr uint8_t fifoEnable = 0x40;
        static constexpr uint8_t latchInterrupt1 = 0x08;
        static constexpr uint8_t dataReadyInterrupt1 = 0x10;
        static constexpr uint8_t watermarkInterrupt1 = 0x04;
        static constexpr uint8_t overrunInterrupt1 = 0x02;
        static constexpr uint8_t interruptActiveLow = 0x02;

        // Contract used by Lsm303dlhcWithPolling
        static constexpr uint8_t dataAvailable = 0x08;

        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;

        void ReadAndDeliverSamples() override;
        void EnableDataReadyInterrupt(bool enable) override;
        void ClearMeasurementCallbacks() override;
        hal::InterruptTrigger DataReadyTrigger() const override;

        void DeliverAcceleration(infra::MemoryRange<const Acceleration> samples);

        static int16_t RawSample(const uint8_t* data);
        Acceleration ToAcceleration(int16_t raw) const;

        bool AccelerometerRequested() const;

        Config config;

    private:
        class AccelerometerAdapter
            : public Accelerometer
        {
        public:
            explicit AccelerometerAdapter(Lsm303dlhcAccelerometer& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            Lsm303dlhcAccelerometer& device;
        };

        void PushConfigurationSteps();
        void VerifyPresence();
        void CompleteInitialization();
        void DeliverMeasurement();

        uint8_t Control1Value(PowerMode mode) const;
        uint8_t Control4Value(PowerMode mode) const;
        uint8_t Control5Value() const;
        uint8_t Control6Value() const;

        AccelerometerAdapter accelerometerAdapter{ *this };

        infra::Function<void(Accelerometer::Samples)> onAccelerometerMeasurement;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onPowerModeSet;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<Acceleration, axisCount> accelerationSamples = {};

        uint8_t scratch = 0;
        PowerMode powerMode = PowerMode::powerDown;
    };
}

#endif
