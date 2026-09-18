#include "drivers/imu/l3gd20/L3gd20Core.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    L3gd20Core::GyroscopeAdapter::GyroscopeAdapter(L3gd20Core& device)
        : device(device)
    {}

    void L3gd20Core::GyroscopeAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        device.onGyroscopeMeasurement = onMeasurement;
        device.UpdateSampling(device.GyroscopeRequested());
    }

    void L3gd20Core::GyroscopeAdapter::Stop()
    {
        device.onGyroscopeMeasurement = nullptr;
        device.UpdateSampling(device.GyroscopeRequested());
    }

    L3gd20Core::L3gd20Core(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin, hal::GpioPin& interruptPin)
        : bus(bus)
        , dataReadyPin(dataReadyPin)
        , interruptPin(interruptPin)
        , dataReadyPinConnected(&dataReadyPin != &hal::dummyPin)
        , interruptPinConnected(&interruptPin != &hal::dummyPin)
        , runner(bus, sharedAccess)
    {}

    L3gd20Core::~L3gd20Core()
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();

        if (interruptPinConnected)
            interruptPin.DisableInterrupt();
    }

    void L3gd20Core::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(!runner.Busy());
        really_assert(config.variant == Variant::l3gd20h || !IsLowOutputDataRate(config.outputDataRate));

        this->config = config;
        onInitialized = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ registerWhoAmI, infra::MakeByteRange(scratch) });
        runner.Push(services::RegisterStepRunner::Invoke{ [this]()
            {
                VerifyIdentification();
            } });
        // Reconfigure with the output stage off, so a reboot cannot race a half written configuration
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl5, boot });
        runner.Push(services::RegisterStepRunner::Delay{ std::chrono::milliseconds(20) });

        PushConfigurationSteps();

        runner.Start([this]()
            {
                CompleteInitialization();
            });
    }

    void L3gd20Core::PushConfigurationSteps()
    {
        if (HasLowOutputDataRateRegister())
            runner.Push(services::RegisterStepRunner::WriteRegister{ registerLowOutputDataRate, LowOutputDataRateValue() });

        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl2, Control2Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl3, Control3Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl4, Control4Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl5, Control5Value() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerReference, config.reference });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerFifoControl, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerInterrupt1Configuration, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(PowerMode::normal) });
        runner.Push(services::RegisterStepRunner::Delay{ config.turnOnTime });
    }

    void L3gd20Core::VerifyIdentification()
    {
        if (scratch == ExpectedIdentification())
            return;

        runner.Abort();

        infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]()
            {
                self->onInitialized(InitializationResult::deviceNotFound);
            });
    }

    void L3gd20Core::CompleteInitialization()
    {
        initialized = true;
        powerMode = PowerMode::normal;

        onInitialized(InitializationResult::success);
    }

    bool L3gd20Core::Initialized() const
    {
        return initialized;
    }

    void L3gd20Core::Stop(const infra::Function<void()>& onDone)
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();

        if (interruptPinConnected)
            interruptPin.DisableInterrupt();

        StopSampling();
        ClearCallbacks();
        sampling = false;
        stopping = true;
        onModified = nullptr;

        runner.Abort();

        onStopped = onDone;
        sharedAccess.SetAction([this]()
            {
                ReportStopped();
            });

        if (!sharedAccess.Referenced())
            infra::EventDispatcher::Instance().Schedule([self = KeepAlive(*this)]() {});
    }

    void L3gd20Core::ReportStopped()
    {
        sharedAccess.SetAction(infra::emptyFunction);

        if (onStopped)
            onStopped();
    }

    void L3gd20Core::ClearCallbacks()
    {
        onGyroscopeMeasurement = nullptr;
    }

    void L3gd20Core::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(!runner.Busy());

        bool waitForTurnOn = powerMode == PowerMode::powerDown && mode != PowerMode::powerDown;
        onSequenceDone = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(mode) });

        if (waitForTurnOn)
            runner.Push(services::RegisterStepRunner::Delay{ config.turnOnTime });

        runner.Start([this, mode]()
            {
                powerMode = mode;
                onSequenceDone();
            });
    }

    L3gd20Core::PowerMode L3gd20Core::CurrentPowerMode() const
    {
        return powerMode;
    }

    void L3gd20Core::SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(config.variant == Variant::l3gd20h || !IsLowOutputDataRate(rate));

        bool switchesBlock = HasLowOutputDataRateRegister() && IsLowOutputDataRate(rate) != IsLowOutputDataRate(config.outputDataRate);

        config.outputDataRate = rate;

        if (!switchesBlock)
            return WriteRegister(registerControl1, Control1Value(powerMode), onDone);

        // The low rate block is only allowed to change while the output stage is off
        really_assert(!runner.Busy());
        onSequenceDone = onDone;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, 0 });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerLowOutputDataRate, LowOutputDataRateValue() });
        runner.Push(services::RegisterStepRunner::WriteRegister{ registerControl1, Control1Value(powerMode) });

        runner.Start([this]()
            {
                onSequenceDone();
            });
    }

    void L3gd20Core::SetBandwidth(Bandwidth bandwidth, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.bandwidth = bandwidth;

        WriteRegister(registerControl1, Control1Value(powerMode), onDone);
    }

    void L3gd20Core::SetFullScale(FullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.fullScale = scale;

        WriteRegister(registerControl4, Control4Value(), onDone);
    }

    void L3gd20Core::SetHighPassFilter(HighPassMode mode, uint8_t cutOff, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.highPassMode = mode;
        config.highPassCutOff = cutOff;

        WriteRegister(registerControl2, Control2Value(), onDone);
    }

    void L3gd20Core::SoftwareReset(const infra::Function<void()>& onDone)
    {
        really_assert(HasLowOutputDataRateRegister());

        WriteRegister(registerLowOutputDataRate, static_cast<uint8_t>(LowOutputDataRateValue() | softwareResetRequest), onDone);
    }

    void L3gd20Core::MeasureTemperature(const infra::Function<void(Temperature)>& onDone)
    {
        really_assert(initialized);
        onTemperature = onDone;

        ReadRegister(registerOutTemperature, infra::MakeByteRange(temperatureBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverTemperature();
            });
    }

    void L3gd20Core::DeliverTemperature()
    {
        onTemperature(ToTemperature(static_cast<int8_t>(temperatureBuffer)));
    }

    L3gd20Core::Gyroscope& L3gd20Core::AsGyroscope()
    {
        return gyroscopeAdapter;
    }

    void L3gd20Core::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        // A continuation may still run after Stop; the device is not touched again once stopped, and
        // Stop reports through sharedAccess when the last transaction in flight releases it
        if (stopping)
            return;

        really_assert(!onRegisterAccessed);
        onRegisterAccessed = onDone;

        bus.ReadRegister(address, data, [self = KeepAlive(*this)]()
            {
                self->onRegisterAccessed();
            });
    }

    void L3gd20Core::WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone)
    {
        if (stopping)
            return;

        really_assert(!onRegisterAccessed);
        writeValue = value;
        onRegisterAccessed = onDone;

        bus.WriteRegister(address, infra::MakeByteRange(writeValue), [self = KeepAlive(*this)]()
            {
                self->onRegisterAccessed();
            });
    }

    void L3gd20Core::ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone)
    {
        really_assert(!onModified);

        modifyAddress = address;
        modifyClearMask = clearMask;
        modifySetMask = setMask;
        onModified = onDone;

        ReadRegister(modifyAddress, infra::MakeByteRange(modifyValue), [self = KeepAlive(*this)]()
            {
                self->WriteRegister(self->modifyAddress, static_cast<uint8_t>((self->modifyValue & ~self->modifyClearMask) | self->modifySetMask), [self]()
                    {
                        self->onModified();
                    });
            });
    }

    void L3gd20Core::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        if (dataReadyPinConnected)
            dataReadyPin.EnableInterrupt(onSampleAvailable, DataReadyTrigger(), hal::InterruptType::dispatched);
    }

    void L3gd20Core::StopSampling()
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();
    }

    void L3gd20Core::UpdateSampling(bool wanted)
    {
        if (wanted == sampling)
            return;

        sampling = wanted;

        if (sampling)
        {
            StartSampling([self = KeepAlive(*this)]()
                {
                    self->ReadAndDeliverSamples();
                });
            EnableDataReadyInterrupt(true);
        }
        else
        {
            StopSampling();
            EnableDataReadyInterrupt(false);
        }
    }

    void L3gd20Core::EnableDataReadyInterrupt(bool enable)
    {
        // The FIFO sources share CTRL_REG3 with data ready, so only that one bit may be touched
        ModifyRegister(registerControl3, dataReadyInterrupt2, enable ? dataReadyInterrupt2 : 0, infra::emptyFunction);
    }

    void L3gd20Core::ReadAndDeliverSamples()
    {
        ReadRegister(registerOutXLow, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
            {
                self->DeliverMeasurement();
            });
    }

    void L3gd20Core::DeliverMeasurement()
    {
        for (std::size_t index = 0; index != angularVelocitySamples.size(); ++index)
            angularVelocitySamples[index] = ToAngularVelocity(RawSample(&measurementBuffer[2 * index]));

        DeliverAngularVelocity(infra::MakeRange(angularVelocitySamples));
    }

    void L3gd20Core::DeliverAngularVelocity(infra::MemoryRange<const AngularVelocity> samples)
    {
        if (onGyroscopeMeasurement)
            onGyroscopeMeasurement(samples);
    }

    int16_t L3gd20Core::RawSample(const uint8_t* data)
    {
        // Little endian while CTRL_REG4.BLE is clear, the opposite of the threshold registers
        return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8) | data[0]);
    }

    L3gd20Core::AngularVelocity L3gd20Core::ToAngularVelocity(int16_t raw) const
    {
        // 8.75, 17.50 and 70.00 milli-degrees per second per count, held in hundredths so the
        // division stays exact. Code three is a second encoding of the largest scale, so a value
        // read back from the device is a legal index as well
        static constexpr std::array<int64_t, 4> hundredthsPerCount = { { 875, 1750, 7000, 7000 } };
        static constexpr int64_t denominator = 100;

        int64_t numerator = static_cast<int64_t>(raw) * hundredthsPerCount[static_cast<uint8_t>(config.fullScale) & 0x03];
        int64_t rounding = numerator >= 0 ? denominator / 2 : -denominator / 2;

        return AngularVelocity{ static_cast<int32_t>((numerator + rounding) / denominator) };
    }

    L3gd20Core::Temperature L3gd20Core::ToTemperature(int8_t raw) const
    {
        return Temperature{ config.temperatureReferenceMilliCelsius - static_cast<int32_t>(raw) * 1000 };
    }

    bool L3gd20Core::GyroscopeRequested() const
    {
        return static_cast<bool>(onGyroscopeMeasurement);
    }

    bool L3gd20Core::Sampling() const
    {
        return sampling;
    }

    bool L3gd20Core::TransactionOutstanding() const
    {
        return static_cast<bool>(onRegisterAccessed);
    }

    bool L3gd20Core::HasLowOutputDataRateRegister() const
    {
        return config.variant == Variant::l3gd20h;
    }

    bool L3gd20Core::HighPassRequired() const
    {
        return config.outputSelection != OutputSelection::lowPassOnly;
    }

    hal::InterruptTrigger L3gd20Core::DataReadyTrigger() const
    {
        // The L3GD20H drives the data ready line from its own level bit, the L3GD20 shares the one
        // active level with the interrupt line
        auto polarity = HasLowOutputDataRateRegister() ? config.dataReadyPolarity : config.interruptPolarity;

        return polarity == InterruptPolarity::activeLow ? hal::InterruptTrigger::fallingEdge : hal::InterruptTrigger::risingEdge;
    }

    hal::InterruptTrigger L3gd20Core::ThresholdInterruptTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? hal::InterruptTrigger::fallingEdge : hal::InterruptTrigger::risingEdge;
    }

    uint8_t L3gd20Core::ExpectedIdentification() const
    {
        return config.expectedIdentification.value_or(HasLowOutputDataRateRegister() ? identificationL3gd20h : identificationL3gd20);
    }

    uint8_t L3gd20Core::Control1Value(PowerMode mode) const
    {
        if (mode == PowerMode::powerDown)
            return 0;

        // Sleep keeps the device powered with every axis deselected
        uint8_t axes = mode == PowerMode::sleep
                           ? 0
                           : static_cast<uint8_t>((config.enableZ ? axisEnableZ : 0) | (config.enableY ? axisEnableY : 0) | (config.enableX ? axisEnableX : 0));

        return static_cast<uint8_t>((DataRateCode(config.outputDataRate) << 6) | (static_cast<uint8_t>(config.bandwidth) << 4) | powerEnable | axes);
    }

    uint8_t L3gd20Core::Control2Value() const
    {
        return static_cast<uint8_t>((static_cast<uint8_t>(config.highPassMode) << 4) | (config.highPassCutOff & 0x0f));
    }

    uint8_t L3gd20Core::Control3Value() const
    {
        return static_cast<uint8_t>((config.interruptPolarity == InterruptPolarity::activeLow ? interruptActiveLow : 0) | (config.interruptDrive == InterruptDrive::openDrain ? openDrain : 0));
    }

    uint8_t L3gd20Core::Control4Value() const
    {
        return static_cast<uint8_t>((config.blockDataUpdate ? blockDataUpdateEnable : 0) | (static_cast<uint8_t>(config.fullScale) << 4) | (config.threeWireSpi ? serialInterfaceMode3Wire : 0));
    }

    uint8_t L3gd20Core::Control5Value() const
    {
        return static_cast<uint8_t>((HighPassRequired() ? highPassEnable : 0) | (static_cast<uint8_t>(config.outputSelection) & 0x03));
    }

    uint8_t L3gd20Core::LowOutputDataRateValue() const
    {
        return static_cast<uint8_t>((IsLowOutputDataRate(config.outputDataRate) ? lowOutputDataRateEnable : 0) | (config.dataReadyPolarity == InterruptPolarity::activeLow ? dataReadyActiveLow : 0) | (config.disableI2cInterface ? i2cDisable : 0));
    }
}
