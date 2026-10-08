#include "drivers/imu/iis2dlpc/Iis2dlpcCore.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Iis2dlpcCore::Iis2dlpcCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : AccelerometerSensor(bus, dataReadyPin)
    {}

    void Iis2dlpcCore::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(!runner.Busy());
        really_assert(IsSupported(config.outputDataRate, config.operatingMode));

        this->config = config;
        onInitialized = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ registerWhoAmI, infra::MakeByteRange(scratch) });
        runner.Push(services::RegisterStepRunner::Invoke{ [this]()
            {
                VerifyIdentification();
            } });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, softReset });
        runner.Push(services::RegisterStepRunner::Delay{ resetDelay });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, Control2Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl3, Control3Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl6, Control6Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(PowerMode::normal) });

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void Iis2dlpcCore::VerifyIdentification()
    {
        if (scratch == identification)
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void Iis2dlpcCore::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    void Iis2dlpcCore::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(!runner.Busy());

        onSequenceDone = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(mode) });

        runner.Start([this, mode]()
            {
                powerMode = mode;
                onSequenceDone();
            });
    }

    Iis2dlpcCore::PowerMode Iis2dlpcCore::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Iis2dlpcCore::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(IsSupported(rate, config.operatingMode));

        config.outputDataRate = rate;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void Iis2dlpcCore::SetOperatingMode(OperatingMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(IsSupported(config.outputDataRate, mode));

        config.operatingMode = mode;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void Iis2dlpcCore::SetFullScale(FullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.fullScale = scale;

        WriteRegister(registerControl6, Control6Value(), onDone);
    }

    void Iis2dlpcCore::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutXLow, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void Iis2dlpcCore::DeliverMeasurement()
    {
        for (std::size_t index = 0; index != axisCount; ++index)
            accelerationSamples[index] = ToAcceleration(RawSample(&measurementBuffer[2 * index]) >> Shift(), MicroGPerCount());

        DeliverAcceleration(infra::MakeRange(accelerationSamples));
    }

    void Iis2dlpcCore::EnableDataReadyInterrupt(bool enable)
    {
        ModifyRegister(registerInterrupt1PadControl, dataReadyOnInterrupt1, enable ? dataReadyOnInterrupt1 : uint8_t{ 0 }, infra::emptyFunction);
    }

    hal::InterruptTrigger Iis2dlpcCore::DataReadyTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow
                   ? hal::InterruptTrigger::fallingEdge
                   : hal::InterruptTrigger::risingEdge;
    }

    uint8_t Iis2dlpcCore::Control1Value(PowerMode mode) const
    {
        uint8_t odr = mode == PowerMode::powerDown
                          ? uint8_t{ 0 }
                          : static_cast<uint8_t>(static_cast<uint8_t>(config.outputDataRate) << 4);
        uint8_t modeBits = config.operatingMode == OperatingMode::highPerformance
                               ? highPerformanceMode
                               : static_cast<uint8_t>(config.operatingMode);

        return static_cast<uint8_t>(odr | modeBits);
    }

    uint8_t Iis2dlpcCore::Control2Value() const
    {
        return static_cast<uint8_t>(addressIncrement | (config.blockDataUpdate ? blockDataUpdateEnable : 0));
    }

    uint8_t Iis2dlpcCore::Control3Value() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? interruptActiveLow : uint8_t{ 0 };
    }

    uint8_t Iis2dlpcCore::Control6Value() const
    {
        return static_cast<uint8_t>(
            (static_cast<uint8_t>(config.filterBandwidth) << 6) |
            (static_cast<uint8_t>(config.fullScale) << 4) |
            (config.lowNoise ? lowNoiseEnable : 0));
    }

    uint8_t Iis2dlpcCore::Shift() const
    {
        return config.operatingMode == OperatingMode::lowPower1 ? uint8_t{ 4 } : uint8_t{ 2 };
    }

    int64_t Iis2dlpcCore::MicroGPerCount() const
    {
        static constexpr std::array<int64_t, 4> table14{ { 244, 488, 976, 1952 } };

        int64_t base = table14[static_cast<uint8_t>(config.fullScale)];

        return config.operatingMode == OperatingMode::lowPower1 ? base * 4 : base;
    }

    bool Iis2dlpcCore::IsSupported(OutputDataRate rate, OperatingMode mode)
    {
        if (mode == OperatingMode::highPerformance)
            return rate != OutputDataRate::millihertz1600LowPower;

        return static_cast<uint8_t>(rate) <= static_cast<uint8_t>(OutputDataRate::hertz200);
    }
}
