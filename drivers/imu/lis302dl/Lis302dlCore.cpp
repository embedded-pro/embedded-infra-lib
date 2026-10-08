#include "drivers/imu/lis302dl/Lis302dlCore.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Lis302dlCore::Lis302dlCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : AccelerometerSensor(bus, dataReadyPin)
    {}

    void Lis302dlCore::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(!runner.Busy());

        this->config = config;
        onInitialized = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ registerWhoAmI, infra::MakeByteRange(scratch) });
        runner.Push(services::RegisterStepRunner::Invoke{ [this]()
            {
                VerifyIdentification();
            } });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, boot });
        runner.Push(services::RegisterStepRunner::Delay{ bootDelay });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl3, Control3Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(PowerMode::normal) });

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void Lis302dlCore::VerifyIdentification()
    {
        if (scratch == identification)
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void Lis302dlCore::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    void Lis302dlCore::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
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

    Lis302dlCore::PowerMode Lis302dlCore::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Lis302dlCore::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.outputDataRate = rate;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void Lis302dlCore::SetFullScale(FullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.fullScale = scale;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void Lis302dlCore::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutX, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void Lis302dlCore::DeliverMeasurement()
    {
        for (std::size_t index = 0; index != axisCount; ++index)
            accelerationSamples[index] = ToAcceleration(static_cast<int8_t>(measurementBuffer[2 * index]), MicroGPerCount(config.fullScale));

        DeliverAcceleration(infra::MakeRange(accelerationSamples));
    }

    void Lis302dlCore::EnableDataReadyInterrupt(bool enable)
    {
        ModifyRegister(registerControl3, interrupt1ConfigurationMask, enable ? dataReadyOnInterrupt1 : uint8_t{ 0 }, infra::emptyFunction);
    }

    hal::InterruptTrigger Lis302dlCore::DataReadyTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow
                   ? hal::InterruptTrigger::fallingEdge
                   : hal::InterruptTrigger::risingEdge;
    }

    uint8_t Lis302dlCore::Control1Value(PowerMode mode) const
    {
        uint8_t value = config.outputDataRate == OutputDataRate::hertz400 ? dataRate400 : uint8_t{ 0 };
        value |= config.fullScale == FullScale::g8 ? fullScale8g : uint8_t{ 0 };
        value |= mode == PowerMode::normal ? deviceActive : uint8_t{ 0 };
        value |= config.enableX ? axisEnableX : uint8_t{ 0 };
        value |= config.enableY ? axisEnableY : uint8_t{ 0 };
        value |= config.enableZ ? axisEnableZ : uint8_t{ 0 };

        return value;
    }

    uint8_t Lis302dlCore::Control3Value() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? interruptActiveLow : uint8_t{ 0 };
    }

    int64_t Lis302dlCore::MicroGPerCount(FullScale scale)
    {
        static constexpr std::array<int64_t, 2> table{ { 18000, 72000 } };

        return table[static_cast<uint8_t>(scale)];
    }
}
