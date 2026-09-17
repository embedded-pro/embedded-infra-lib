#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometer.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Lsm303dlhcAccelerometer::AccelerometerAdapter::AccelerometerAdapter(Lsm303dlhcAccelerometer& device)
        : device(device)
    {}

    void Lsm303dlhcAccelerometer::AccelerometerAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        device.onAccelerometerMeasurement = onMeasurement;
        device.UpdateSampling(device.AccelerometerRequested());
    }

    void Lsm303dlhcAccelerometer::AccelerometerAdapter::Stop()
    {
        device.onAccelerometerMeasurement = nullptr;
        device.UpdateSampling(device.AccelerometerRequested());
    }

    Lsm303dlhcAccelerometer::Lsm303dlhcAccelerometer(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : Lsm303dlhcSensor(bus, dataReadyPin)
    {}

    void Lsm303dlhcAccelerometer::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(!runner.Busy());

        this->config = config;
        onInitialized = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl5, boot });
        runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(5) });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(PowerMode::normal) });
        runner.Push(services::RegisterStepRunner::ReadBurst{ registerControl1, infra::MakeByteRange(scratch) });
        runner.Push(services::RegisterStepRunner::Invoke{ [this]()
            {
                VerifyPresence();
            } });

        PushConfigurationSteps();

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void Lsm303dlhcAccelerometer::PushConfigurationSteps()
    {
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl3, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, Control4Value(PowerMode::normal) });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl5, Control5Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl6, Control6Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerReference, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerFifoControl, 0 });
        runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(1) });
    }

    void Lsm303dlhcAccelerometer::VerifyPresence()
    {
        if (scratch == Control1Value(PowerMode::normal))
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void Lsm303dlhcAccelerometer::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    void Lsm303dlhcAccelerometer::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(!runner.Busy());

        bool waitForTurnOn = powerMode == PowerMode::powerDown && mode != PowerMode::powerDown;
        onPowerModeSet = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(mode) });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, Control4Value(mode) });

        if (waitForTurnOn)
            runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(5) });

        runner.Start([this, mode]()
            {
                powerMode = mode;
                onPowerModeSet();
            });
    }

    Lsm303dlhcAccelerometer::PowerMode Lsm303dlhcAccelerometer::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Lsm303dlhcAccelerometer::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.outputDataRate = rate;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void Lsm303dlhcAccelerometer::SetFullScale(FullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.fullScale = scale;

        WriteRegister(registerControl4, Control4Value(powerMode), onDone);
    }

    void Lsm303dlhcAccelerometer::SetHighResolution(bool highResolution, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.highResolution = highResolution;

        WriteRegister(registerControl4, Control4Value(powerMode), onDone);
    }

    Lsm303dlhcAccelerometer::Accelerometer& Lsm303dlhcAccelerometer::AsAccelerometer()
    {
        return accelerometerAdapter;
    }

    void Lsm303dlhcAccelerometer::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutXLow, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void Lsm303dlhcAccelerometer::EnableDataReadyInterrupt(bool enable)
    {
        // Only the data ready bit is touched, so the FIFO watermark and overrun sources that
        // Lsm303dlhcAccelerometerWithFifo may have enabled in the same register survive
        ModifyRegister(registerControl3, dataReadyInterrupt1, enable ? dataReadyInterrupt1 : uint8_t(0), infra::emptyFunction);
    }

    void Lsm303dlhcAccelerometer::ClearMeasurementCallbacks()
    {
        onAccelerometerMeasurement = nullptr;
    }

    hal::InterruptTrigger Lsm303dlhcAccelerometer::DataReadyTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? hal::InterruptTrigger::fallingEdge : hal::InterruptTrigger::risingEdge;
    }

    void Lsm303dlhcAccelerometer::DeliverMeasurement()
    {
        for (std::size_t index = 0; index != accelerationSamples.size(); ++index)
            accelerationSamples[index] = ToAcceleration(RawSample(&measurementBuffer[2 * index]));

        DeliverAcceleration(infra::MakeRange(accelerationSamples));
    }

    void Lsm303dlhcAccelerometer::DeliverAcceleration(infra::MemoryRange<const Acceleration> samples)
    {
        if (onAccelerometerMeasurement)
            onAccelerometerMeasurement(samples);
    }

    int16_t Lsm303dlhcAccelerometer::RawSample(const uint8_t* data)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8) | data[0]);
    }

    Lsm303dlhcAccelerometer::Acceleration Lsm303dlhcAccelerometer::ToAcceleration(int16_t raw) const
    {
        // The output is left-justified whatever the resolution, so the top twelve bits always carry
        // the count that the high-resolution sensitivity applies to
        static constexpr std::array<int64_t, 4> milliGPerCount = { { 1, 2, 4, 12 } };
        static constexpr int64_t denominator = 100000;

        int64_t numerator = static_cast<int64_t>(raw >> 4) * milliGPerCount[static_cast<uint8_t>(config.fullScale)] * 980665;
        int64_t rounding = numerator >= 0 ? denominator / 2 : -denominator / 2;

        return Acceleration{ static_cast<int32_t>((numerator + rounding) / denominator) };
    }

    bool Lsm303dlhcAccelerometer::AccelerometerRequested() const
    {
        return static_cast<bool>(onAccelerometerMeasurement);
    }

    uint8_t Lsm303dlhcAccelerometer::Control1Value(PowerMode mode) const
    {
        uint8_t rate = mode == PowerMode::powerDown ? 0 : static_cast<uint8_t>(config.outputDataRate);
        uint8_t axes = static_cast<uint8_t>((config.enableX ? 0x01 : 0x00) | (config.enableY ? 0x02 : 0x00) | (config.enableZ ? 0x04 : 0x00));

        return static_cast<uint8_t>((rate << 4) | (mode == PowerMode::lowPower ? lowPowerEnable : 0x00) | axes);
    }

    uint8_t Lsm303dlhcAccelerometer::Control4Value(PowerMode mode) const
    {
        return static_cast<uint8_t>((config.blockDataUpdate ? blockDataUpdateEnable : 0x00) | (static_cast<uint8_t>(config.fullScale) << 4) | (config.highResolution && mode != PowerMode::lowPower ? highResolutionEnable : 0x00));
    }

    uint8_t Lsm303dlhcAccelerometer::Control5Value() const
    {
        return config.latchInterrupt ? latchInterrupt1 : uint8_t(0);
    }

    uint8_t Lsm303dlhcAccelerometer::Control6Value() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? interruptActiveLow : uint8_t(0);
    }
}
