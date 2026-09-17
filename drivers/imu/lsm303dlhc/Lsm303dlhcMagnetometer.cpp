#include "drivers/imu/lsm303dlhc/Lsm303dlhcMagnetometer.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Lsm303dlhcMagnetometer::MagnetometerAdapter::MagnetometerAdapter(Lsm303dlhcMagnetometer& device)
        : device(device)
    {}

    void Lsm303dlhcMagnetometer::MagnetometerAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        really_assert(device.config.mode == Mode::continuous);

        device.onMagnetometerMeasurement = onMeasurement;
        device.UpdateSampling(device.MagnetometerRequested());
    }

    void Lsm303dlhcMagnetometer::MagnetometerAdapter::Stop()
    {
        device.onMagnetometerMeasurement = nullptr;
        device.UpdateSampling(device.MagnetometerRequested());
    }

    Lsm303dlhcMagnetometer::Lsm303dlhcMagnetometer(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : Lsm303dlhcSensor(bus, dataReadyPin)
    {}

    void Lsm303dlhcMagnetometer::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(!runner.Busy());

        this->config = config;
        onInitialized = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ registerIdentificationA, infra::MakeRange(identification) });
        runner.Push(services::RegisterStepRunner::Invoke{ [this]()
            {
                VerifyIdentification();
            } });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerConfigurationA, ConfigurationAValue() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerConfigurationB, ConfigurationBValue() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerMode, static_cast<uint8_t>(this->config.mode) });
        runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(5) });

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void Lsm303dlhcMagnetometer::VerifyIdentification()
    {
        if (identification == config.expectedIdentification)
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void Lsm303dlhcMagnetometer::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    void Lsm303dlhcMagnetometer::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(!runner.Busy());

        bool waitForTurnOn = powerMode == PowerMode::sleep && mode != PowerMode::sleep;
        onPowerModeSet = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerMode, mode == PowerMode::sleep ? static_cast<uint8_t>(Mode::sleep) : static_cast<uint8_t>(config.mode) });

        if (waitForTurnOn)
            runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(5) });

        runner.Start([this, mode]()
            {
                powerMode = mode;
                onPowerModeSet();
            });
    }

    Lsm303dlhcMagnetometer::PowerMode Lsm303dlhcMagnetometer::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Lsm303dlhcMagnetometer::SetGain(Gain gain, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.gain = gain;

        WriteRegister(registerConfigurationB, ConfigurationBValue(), onDone);
    }

    void Lsm303dlhcMagnetometer::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.outputDataRate = rate;

        WriteRegister(registerConfigurationA, ConfigurationAValue(), onDone);
    }

    void Lsm303dlhcMagnetometer::SetMode(Mode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.mode = mode;

        WriteRegister(registerMode, static_cast<uint8_t>(mode), onDone);
    }

    void Lsm303dlhcMagnetometer::MeasureTemperature(const infra::Function<void(Temperature)>& onDone)
    {
        really_assert(initialized);
        really_assert(config.temperatureEnabled);

        onTemperature = onDone;

        ReadRegister(registerTemperatureOutHigh, infra::MakeRange(temperatureBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverTemperature();
            });
    }

    bool Lsm303dlhcMagnetometer::Saturated() const
    {
        return saturated;
    }

    Lsm303dlhcMagnetometer::Magnetometer& Lsm303dlhcMagnetometer::AsMagnetometer()
    {
        return magnetometerAdapter;
    }

    void Lsm303dlhcMagnetometer::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutXHigh, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void Lsm303dlhcMagnetometer::EnableDataReadyInterrupt(bool)
    {}

    void Lsm303dlhcMagnetometer::ClearMeasurementCallbacks()
    {
        onMagnetometerMeasurement = nullptr;
    }

    void Lsm303dlhcMagnetometer::DeliverMeasurement()
    {
        // The output registers run X, Z, Y rather than X, Y, Z
        int16_t x = RawSample(&measurementBuffer[0]);
        int16_t z = RawSample(&measurementBuffer[2]);
        int16_t y = RawSample(&measurementBuffer[4]);

        saturated = x == saturatedSample || y == saturatedSample || z == saturatedSample;

        magneticFluxDensitySamples[0] = ToMagneticFluxDensity(x, Axis::xy);
        magneticFluxDensitySamples[1] = ToMagneticFluxDensity(y, Axis::xy);
        magneticFluxDensitySamples[2] = ToMagneticFluxDensity(z, Axis::z);

        DeliverMagneticFluxDensity(infra::MakeRange(magneticFluxDensitySamples));
    }

    void Lsm303dlhcMagnetometer::DeliverTemperature()
    {
        onTemperature(ToTemperature(RawSample(temperatureBuffer.data())));
    }

    void Lsm303dlhcMagnetometer::DeliverMagneticFluxDensity(infra::MemoryRange<const MagneticFluxDensity> samples)
    {
        if (onMagnetometerMeasurement)
            onMagnetometerMeasurement(samples);
    }

    int16_t Lsm303dlhcMagnetometer::RawSample(const uint8_t* data)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(data[0]) << 8) | data[1]);
    }

    Lsm303dlhcMagnetometer::MagneticFluxDensity Lsm303dlhcMagnetometer::ToMagneticFluxDensity(int16_t raw, Axis axis) const
    {
        // The Z axis has its own sensitivity, unlike the X and Y axes
        static constexpr std::array<int64_t, 8> countsPerGaussXy = { { 0, 1100, 855, 670, 450, 400, 330, 230 } };
        static constexpr std::array<int64_t, 8> countsPerGaussZ = { { 0, 980, 760, 600, 400, 355, 295, 205 } };

        int64_t denominator = (axis == Axis::z ? countsPerGaussZ : countsPerGaussXy)[static_cast<uint8_t>(config.gain)];
        int64_t numerator = static_cast<int64_t>(raw) * 1000;
        int64_t rounding = numerator >= 0 ? denominator / 2 : -denominator / 2;

        return MagneticFluxDensity{ static_cast<int32_t>((numerator + rounding) / denominator) };
    }

    Lsm303dlhcMagnetometer::Temperature Lsm303dlhcMagnetometer::ToTemperature(int16_t raw)
    {
        // Eight counts per degree Celsius on the left-justified twelve bit value. The datasheet
        // gives no reference point, so this is a relative reading and carries no offset
        return Temperature{ static_cast<int32_t>(raw >> 4) * 125 };
    }

    bool Lsm303dlhcMagnetometer::MagnetometerRequested() const
    {
        return static_cast<bool>(onMagnetometerMeasurement);
    }

    uint8_t Lsm303dlhcMagnetometer::ConfigurationAValue() const
    {
        return static_cast<uint8_t>((config.temperatureEnabled ? temperatureEnable : 0x00) | (static_cast<uint8_t>(config.outputDataRate) << 2));
    }

    uint8_t Lsm303dlhcMagnetometer::ConfigurationBValue() const
    {
        return static_cast<uint8_t>(static_cast<uint8_t>(config.gain) << 5);
    }
}
