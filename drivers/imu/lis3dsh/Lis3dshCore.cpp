#include "drivers/imu/lis3dsh/Lis3dshCore.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Lis3dshCore::Lis3dshCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : AccelerometerSensor(bus, dataReadyPin)
    {}

    void Lis3dshCore::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
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
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl6, boot });
        runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(10) });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl6, addressIncrement });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl5, Control5Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl3, Control3Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, Control4Value(PowerMode::normal) });

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void Lis3dshCore::VerifyIdentification()
    {
        if (scratch == identification)
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void Lis3dshCore::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    void Lis3dshCore::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(!runner.Busy());

        onSequenceDone = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, Control4Value(mode) });

        runner.Start([this, mode]()
            {
                powerMode = mode;
                onSequenceDone();
            });
    }

    Lis3dshCore::PowerMode Lis3dshCore::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Lis3dshCore::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.outputDataRate = rate;

        WriteRegister(registerControl4, Control4Value(powerMode), onDone);
    }

    void Lis3dshCore::SetFullScale(FullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.fullScale = scale;

        WriteRegister(registerControl5, Control5Value(), onDone);
    }

    void Lis3dshCore::SetBandwidth(Bandwidth bandwidth, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);

        config.bandwidth = bandwidth;

        WriteRegister(registerControl5, Control5Value(), onDone);
    }

    void Lis3dshCore::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutXLow, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void Lis3dshCore::DeliverMeasurement()
    {
        for (std::size_t index = 0; index != accelerationSamples.size(); ++index)
            accelerationSamples[index] = ToAcceleration(RawSample(&measurementBuffer[2 * index]), MicroGPerCount(config.fullScale));

        DeliverAcceleration(infra::MakeRange(accelerationSamples));
    }

    void Lis3dshCore::EnableDataReadyInterrupt(bool enable)
    {
        ModifyRegister(registerControl3, dataReadyEnable, enable ? dataReadyEnable : uint8_t{ 0 }, infra::emptyFunction);
    }

    hal::InterruptTrigger Lis3dshCore::DataReadyTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow
                   ? hal::InterruptTrigger::fallingEdge
                   : hal::InterruptTrigger::risingEdge;
    }

    uint8_t Lis3dshCore::Control3Value() const
    {
        return config.interruptPolarity == InterruptPolarity::activeHigh ? interruptActiveHigh : uint8_t{ 0 };
    }

    uint8_t Lis3dshCore::Control4Value(PowerMode mode) const
    {
        uint8_t rate = mode == PowerMode::powerDown ? uint8_t{ 0 } : static_cast<uint8_t>(config.outputDataRate);
        uint8_t axes = static_cast<uint8_t>((config.enableX ? axisEnableX : 0) | (config.enableY ? axisEnableY : 0) | (config.enableZ ? axisEnableZ : 0));

        return static_cast<uint8_t>((rate << 4) | (config.blockDataUpdate ? blockDataUpdateEnable : 0) | axes);
    }

    uint8_t Lis3dshCore::Control5Value() const
    {
        return static_cast<uint8_t>(
            (static_cast<uint8_t>(config.bandwidth) << 6) |
            (static_cast<uint8_t>(config.fullScale) << 3));
    }

    int64_t Lis3dshCore::MicroGPerCount(FullScale scale)
    {
        static constexpr std::array<int64_t, 5> table{ { 61, 122, 183, 244, 732 } };

        return table[static_cast<uint8_t>(scale)];
    }
}
