#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Mpu9250Core::AccelerometerAdapter::AccelerometerAdapter(Mpu9250Core& device)
        : device(device)
    {}

    void Mpu9250Core::AccelerometerAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        device.onAccelerometerMeasurement = onMeasurement;
        device.UpdateSampling();
    }

    void Mpu9250Core::AccelerometerAdapter::Stop()
    {
        device.onAccelerometerMeasurement = nullptr;
        device.UpdateSampling();
    }

    Mpu9250Core::GyroscopeAdapter::GyroscopeAdapter(Mpu9250Core& device)
        : device(device)
    {}

    void Mpu9250Core::GyroscopeAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        device.onGyroscopeMeasurement = onMeasurement;
        device.UpdateSampling();
    }

    void Mpu9250Core::GyroscopeAdapter::Stop()
    {
        device.onGyroscopeMeasurement = nullptr;
        device.UpdateSampling();
    }

    Mpu9250Core::Mpu9250Core(Mpu9250BusAccess& bus, hal::GpioPin& dataReadyPin)
        : bus(bus)
        , dataReadyPin(dataReadyPin)
        , dataReadyPinConnected(&dataReadyPin != &hal::dummyPin)
    {}

    void Mpu9250Core::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        really_assert(sequencer.Finished());

        this->config = config;
        onInitialized = onDone;

        sequencer.Load([this]()
            {
                sequencer.Step([this]()
                    {
                        ReadRegister(registerWhoAmI, infra::MakeByteRange(scratch), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.If([this]()
                    {
                        return scratch == this->config.expectedWhoAmI;
                    });
                sequencer.Step([this]()
                    {
                        WriteRegister(registerPowerManagement1, deviceReset, [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.Step([this]()
                    {
                        delayTimer.Start(std::chrono::milliseconds(100), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.If([this]()
                    {
                        return bus.RequiresI2cSlaveInterfaceDisabled();
                    });
                sequencer.Step([this]()
                    {
                        WriteRegister(registerUserControl, i2cInterfaceDisable, [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.EndIf();
                ConfigurationSteps();
                sequencer.Execute([this]()
                    {
                        initialized = true;
                        powerMode = PowerMode::normal;
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onInitialized(InitializationResult::success);
                            });
                    });
                sequencer.Else();
                sequencer.Execute([this]()
                    {
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onInitialized(InitializationResult::deviceNotFound);
                            });
                    });
                sequencer.EndIf();
            });
    }

    void Mpu9250Core::ConfigurationSteps()
    {
        sequencer.Step([this]()
            {
                WriteRegister(registerPowerManagement1, static_cast<uint8_t>(config.clockSource), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                delayTimer.Start(std::chrono::milliseconds(1), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerPowerManagement2, 0, [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerSampleRateDivider, config.sampleRateDivider, [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerConfiguration, static_cast<uint8_t>(config.gyroscopeLowPassFilter), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerGyroscopeConfig, static_cast<uint8_t>(static_cast<uint8_t>(config.gyroscopeFullScale) << 3), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerAccelerometerConfig, static_cast<uint8_t>(static_cast<uint8_t>(config.accelerometerFullScale) << 3), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerAccelerometerConfig2, static_cast<uint8_t>(config.accelerometerLowPassFilter), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerInterruptPinConfig, InterruptPinConfigValue(), [this]()
                    {
                        sequencer.Continue();
                    });
            });
        sequencer.Step([this]()
            {
                WriteRegister(registerInterruptEnable, 0, [this]()
                    {
                        sequencer.Continue();
                    });
            });
    }

    void Mpu9250Core::SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        really_assert(sequencer.Finished());

        requestedPowerMode = mode;
        waitForStartUp = powerMode == PowerMode::sleep && mode != PowerMode::sleep;
        onPowerModeSet = onDone;

        sequencer.Load([this]()
            {
                sequencer.Step([this]()
                    {
                        WriteRegister(registerPowerManagement2, PowerManagement2Value(requestedPowerMode), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.Step([this]()
                    {
                        WriteRegister(registerPowerManagement1, PowerManagement1Value(requestedPowerMode), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.If([this]()
                    {
                        return waitForStartUp;
                    });
                sequencer.Step([this]()
                    {
                        delayTimer.Start(std::chrono::milliseconds(35), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
                sequencer.EndIf();
                sequencer.Execute([this]()
                    {
                        powerMode = requestedPowerMode;
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onPowerModeSet();
                            });
                    });
            });
    }

    Mpu9250Core::PowerMode Mpu9250Core::CurrentPowerMode() const
    {
        return powerMode;
    }

    void Mpu9250Core::SetAccelerometerFullScale(AccelerometerFullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.accelerometerFullScale = scale;

        WriteRegister(registerAccelerometerConfig, static_cast<uint8_t>(static_cast<uint8_t>(scale) << 3), onDone);
    }

    void Mpu9250Core::SetGyroscopeFullScale(GyroscopeFullScale scale, const infra::Function<void()>& onDone)
    {
        really_assert(initialized);
        config.gyroscopeFullScale = scale;

        WriteRegister(registerGyroscopeConfig, static_cast<uint8_t>(static_cast<uint8_t>(scale) << 3), onDone);
    }

    void Mpu9250Core::MeasureTemperature(const infra::Function<void(Temperature)>& onDone)
    {
        really_assert(initialized);
        onTemperature = onDone;

        ReadRegister(registerTemperatureOutHigh, infra::MakeRange(temperatureBuffer), [this]()
            {
                onTemperature(ToTemperature(RawSample(temperatureBuffer.data())));
            });
    }

    Mpu9250Core::Accelerometer& Mpu9250Core::AsAccelerometer()
    {
        return accelerometerAdapter;
    }

    Mpu9250Core::Gyroscope& Mpu9250Core::AsGyroscope()
    {
        return gyroscopeAdapter;
    }

    void Mpu9250Core::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        bus.ReadRegister(address, data, onDone);
    }

    void Mpu9250Core::WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone)
    {
        writeValue = value;
        bus.WriteRegister(address, infra::MakeByteRange(writeValue), onDone);
    }

    void Mpu9250Core::ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone)
    {
        really_assert(!onModified);

        modifyAddress = address;
        modifyClearMask = clearMask;
        modifySetMask = setMask;
        onModified = onDone;

        bus.ReadRegister(modifyAddress, infra::MakeByteRange(modifyValue), [this]()
            {
                WriteRegister(modifyAddress, static_cast<uint8_t>((modifyValue & ~modifyClearMask) | modifySetMask), [this]()
                    {
                        onModified();
                    });
            });
    }

    void Mpu9250Core::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        if (dataReadyPinConnected)
            dataReadyPin.EnableInterrupt(onSampleAvailable, DataReadyTrigger(), hal::InterruptType::dispatched);
    }

    void Mpu9250Core::StopSampling()
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();
    }

    void Mpu9250Core::ReadAndDeliverSamples()
    {
        ReadRegister(registerAccelerometerXOutHigh, infra::MakeRange(measurementBuffer), [this]()
            {
                for (std::size_t index = 0; index != accelerationSamples.size(); ++index)
                    accelerationSamples[index] = ToAcceleration(RawSample(&measurementBuffer[2 * index]));

                for (std::size_t index = 0; index != angularVelocitySamples.size(); ++index)
                    angularVelocitySamples[index] = ToAngularVelocity(RawSample(&measurementBuffer[8 + 2 * index]));

                DeliverAcceleration(infra::MakeRange(accelerationSamples));
                DeliverAngularVelocity(infra::MakeRange(angularVelocitySamples));
            });
    }

    void Mpu9250Core::DeliverAcceleration(infra::MemoryRange<const Acceleration> samples)
    {
        if (onAccelerometerMeasurement)
            onAccelerometerMeasurement(samples);
    }

    void Mpu9250Core::DeliverAngularVelocity(infra::MemoryRange<const AngularVelocity> samples)
    {
        if (onGyroscopeMeasurement)
            onGyroscopeMeasurement(samples);
    }

    int16_t Mpu9250Core::RawSample(const uint8_t* data)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(data[0]) << 8) | data[1]);
    }

    Mpu9250Core::Acceleration Mpu9250Core::ToAcceleration(int16_t raw) const
    {
        static constexpr std::array<int32_t, 4> sensitivity = { { 16384, 8192, 4096, 2048 } };

        int64_t numerator = static_cast<int64_t>(raw) * 980665;
        int64_t denominator = 100 * static_cast<int64_t>(sensitivity[static_cast<uint8_t>(config.accelerometerFullScale)]);
        int64_t rounding = numerator >= 0 ? denominator / 2 : -denominator / 2;

        return Acceleration{ static_cast<int32_t>((numerator + rounding) / denominator) };
    }

    Mpu9250Core::AngularVelocity Mpu9250Core::ToAngularVelocity(int16_t raw) const
    {
        // 32768 counts map exactly onto the full scale, so every scale reduces to 15625 / 2^n milli-degrees per count
        static constexpr std::array<int32_t, 4> denominators = { { 2048, 1024, 512, 256 } };

        int64_t denominator = denominators[static_cast<uint8_t>(config.gyroscopeFullScale)];
        int64_t numerator = static_cast<int64_t>(raw) * 15625;
        int64_t rounding = numerator >= 0 ? denominator / 2 : -denominator / 2;

        return AngularVelocity{ static_cast<int32_t>((numerator + rounding) / denominator) };
    }

    Mpu9250Core::Temperature Mpu9250Core::ToTemperature(int16_t raw)
    {
        static constexpr int64_t sensitivityPerHundredThousand = 33387;

        int64_t numerator = static_cast<int64_t>(raw) * 100000;
        int64_t rounding = numerator >= 0 ? sensitivityPerHundredThousand / 2 : -sensitivityPerHundredThousand / 2;

        return Temperature{ static_cast<int32_t>((numerator + rounding) / sensitivityPerHundredThousand + 21000) };
    }

    bool Mpu9250Core::AccelerometerRequested() const
    {
        return static_cast<bool>(onAccelerometerMeasurement);
    }

    bool Mpu9250Core::GyroscopeRequested() const
    {
        return static_cast<bool>(onGyroscopeMeasurement);
    }

    bool Mpu9250Core::Sampling() const
    {
        return sampling;
    }

    hal::InterruptTrigger Mpu9250Core::DataReadyTrigger() const
    {
        return config.interruptPolarity == InterruptPolarity::activeLow ? hal::InterruptTrigger::fallingEdge : hal::InterruptTrigger::risingEdge;
    }

    void Mpu9250Core::UpdateSampling()
    {
        bool wanted = AccelerometerRequested() || GyroscopeRequested();

        if (wanted == sampling)
            return;

        sampling = wanted;

        if (sampling)
        {
            StartSampling([this]()
                {
                    ReadAndDeliverSamples();
                });
            WriteRegister(registerInterruptEnable, rawDataReadyInterrupt, infra::emptyFunction);
        }
        else
        {
            StopSampling();
            WriteRegister(registerInterruptEnable, 0, infra::emptyFunction);
        }
    }

    uint8_t Mpu9250Core::InterruptPinConfigValue() const
    {
        return static_cast<uint8_t>((static_cast<uint8_t>(config.interruptPolarity) << 7) | (static_cast<uint8_t>(config.interruptDrive) << 6) | (static_cast<uint8_t>(config.interruptLatch) << 5) | (config.clearInterruptOnAnyRead ? 0x10 : 0x00));
    }

    uint8_t Mpu9250Core::PowerManagement1Value(PowerMode mode) const
    {
        uint8_t value = static_cast<uint8_t>(config.clockSource);

        if (mode == PowerMode::sleep)
            value |= sleepEnable;
        else if (mode == PowerMode::standby)
            value |= gyroscopeStandby;

        return value;
    }

    uint8_t Mpu9250Core::PowerManagement2Value(PowerMode mode)
    {
        return mode == PowerMode::standby ? allAxesDisabled : 0x00;
    }
}
