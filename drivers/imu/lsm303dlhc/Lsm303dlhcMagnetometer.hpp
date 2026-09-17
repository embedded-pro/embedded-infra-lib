#ifndef DRIVERS_IMU_LSM303DLHC_LSM303DLHC_MAGNETOMETER_HPP
#define DRIVERS_IMU_LSM303DLHC_LSM303DLHC_MAGNETOMETER_HPP

#include "drivers/imu/lsm303dlhc/Lsm303dlhcSensor.hpp"
#include "hal/interfaces/Magnetometer.hpp"
#include "infra/util/Unit.hpp"
#include <array>

namespace drivers
{
    // The magnetic block drives its data-ready line free-running from the output data rate; there is
    // no interrupt enable to program. Without a data-ready pin, compose with Lsm303dlhcWithPolling.
    class Lsm303dlhcMagnetometer
        : public Lsm303dlhcSensor
    {
    public:
        using MagneticFluxDensity = infra::Quantity<infra::MilliGauss, int32_t>;
        using Temperature = infra::Quantity<infra::MilliCelsius, int32_t>;
        using Magnetometer = hal::Magnetometer<infra::MilliGauss, int32_t>;

        enum class Gain : uint8_t
        {
            milliGauss1300 = 1,
            milliGauss1900 = 2,
            milliGauss2500 = 3,
            milliGauss4000 = 4,
            milliGauss4700 = 5,
            milliGauss5600 = 6,
            milliGauss8100 = 7
        };

        enum class OutputDataRate : uint8_t
        {
            milliHertz750 = 0,
            milliHertz1500 = 1,
            milliHertz3000 = 2,
            milliHertz7500 = 3,
            milliHertz15000 = 4,
            milliHertz30000 = 5,
            milliHertz75000 = 6,
            milliHertz220000 = 7
        };

        enum class Mode : uint8_t
        {
            continuous = 0,
            single = 1,
            sleep = 2
        };

        enum class PowerMode : uint8_t
        {
            normal,
            sleep
        };

        struct Config
        {
            Gain gain = Gain::milliGauss1300;
            OutputDataRate outputDataRate = OutputDataRate::milliHertz15000;
            Mode mode = Mode::continuous;
            bool temperatureEnabled = true;
            std::array<uint8_t, 3> expectedIdentification = { { 0x48, 0x34, 0x33 } };
        };

        explicit Lsm303dlhcMagnetometer(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);
        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetGain(Gain gain, const infra::Function<void()>& onDone);
        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetMode(Mode mode, const infra::Function<void()>& onDone);

        void MeasureTemperature(const infra::Function<void(Temperature)>& onDone);

        // Valid during the measurement callback; the device reports -4096 on an axis that overflowed
        bool Saturated() const;

        Magnetometer& AsMagnetometer();

    protected:
        static constexpr uint8_t registerConfigurationA = 0x00;
        static constexpr uint8_t registerConfigurationB = 0x01;
        static constexpr uint8_t registerMode = 0x02;
        static constexpr uint8_t registerOutXHigh = 0x03;
        static constexpr uint8_t registerStatus = 0x09;
        static constexpr uint8_t registerIdentificationA = 0x0a;
        static constexpr uint8_t registerTemperatureOutHigh = 0x31;

        static constexpr uint8_t temperatureEnable = 0x80;

        // Contract used by Lsm303dlhcWithPolling
        static constexpr uint8_t dataAvailable = 0x01;

        static constexpr int16_t saturatedSample = -4096;
        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t identificationSize = 3;
        static constexpr std::size_t axisCount = 3;

        enum class Axis : uint8_t
        {
            xy,
            z
        };

        void ReadAndDeliverSamples() override;
        void EnableDataReadyInterrupt(bool enable) override;
        void ClearMeasurementCallbacks() override;

        void DeliverMagneticFluxDensity(infra::MemoryRange<const MagneticFluxDensity> samples);

        static int16_t RawSample(const uint8_t* data);
        MagneticFluxDensity ToMagneticFluxDensity(int16_t raw, Axis axis) const;
        static Temperature ToTemperature(int16_t raw);

        bool MagnetometerRequested() const;

        Config config;

    private:
        class MagnetometerAdapter
            : public Magnetometer
        {
        public:
            explicit MagnetometerAdapter(Lsm303dlhcMagnetometer& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            Lsm303dlhcMagnetometer& device;
        };

        void VerifyIdentification();
        void CompleteInitialization();
        void DeliverMeasurement();
        void DeliverTemperature();

        uint8_t ConfigurationAValue() const;
        uint8_t ConfigurationBValue() const;

        MagnetometerAdapter magnetometerAdapter{ *this };

        infra::Function<void(Magnetometer::Samples)> onMagnetometerMeasurement;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onPowerModeSet;
        infra::AutoResetFunction<void(Temperature)> onTemperature;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<uint8_t, identificationSize> identification = {};
        std::array<uint8_t, 2> temperatureBuffer = {};
        std::array<MagneticFluxDensity, axisCount> magneticFluxDensitySamples = {};

        PowerMode powerMode = PowerMode::sleep;
        bool saturated = false;
    };
}

#endif
