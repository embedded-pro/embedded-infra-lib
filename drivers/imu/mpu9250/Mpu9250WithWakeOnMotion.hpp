#ifndef DRIVERS_IMU_MPU9250_MPU9250_WITH_WAKE_ON_MOTION_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_WITH_WAKE_ON_MOTION_HPP

#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    template<class Base>
    class Mpu9250WithWakeOnMotion
        : public Base
    {
    public:
        using Base::Base;

        enum class LowPowerOutputDataRate : uint8_t
        {
            milliHertz240 = 0,
            milliHertz490 = 1,
            milliHertz980 = 2,
            milliHertz1950 = 3,
            milliHertz3910 = 4,
            milliHertz7810 = 5,
            milliHertz15630 = 6,
            milliHertz31250 = 7,
            milliHertz62500 = 8,
            milliHertz125000 = 9,
            milliHertz250000 = 10,
            milliHertz500000 = 11
        };

        void EnableWakeOnMotion(uint16_t thresholdMilliG, LowPowerOutputDataRate outputDataRate, const infra::Function<void()>& onMotion, const infra::Function<void()>& onDone);
        void DisableWakeOnMotion(const infra::Function<void()>& onDone);

    private:
        void OnMotionInterrupt();
        uint8_t ThresholdRegisterValue() const;

        static constexpr uint8_t accelerometerIntelligenceEnable = 0x80;
        static constexpr uint8_t accelerometerIntelligenceMode = 0x40;
        static constexpr uint8_t gyroscopeAxesDisabled = 0x07;
        static constexpr uint8_t lowPowerAccelerometerFilter = 0x01;
        static constexpr uint16_t thresholdStepMilliG = 4;
        static constexpr uint16_t maximumThresholdMilliG = 1020;

        infra::AutoResetFunction<void()> onWakeOnMotionConfigured;
        infra::Function<void()> onMotion;
        uint16_t thresholdMilliG = 0;
        LowPowerOutputDataRate outputDataRate = LowPowerOutputDataRate::milliHertz15630;
        uint8_t interruptStatus = 0;
    };

    ////    Implementation    ////

    template<class Base>
    void Mpu9250WithWakeOnMotion<Base>::EnableWakeOnMotion(uint16_t thresholdMilliG, LowPowerOutputDataRate outputDataRate, const infra::Function<void()>& onMotion, const infra::Function<void()>& onDone)
    {
        really_assert(this->sequencer.Finished());
        really_assert(!this->Sampling());

        this->thresholdMilliG = thresholdMilliG;
        this->outputDataRate = outputDataRate;
        this->onMotion = onMotion;
        onWakeOnMotionConfigured = onDone;

        this->sequencer.Load([this]()
            {
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerPowerManagement1, static_cast<uint8_t>(this->config.clockSource), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerPowerManagement2, gyroscopeAxesDisabled, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerAccelerometerConfig2, lowPowerAccelerometerFilter, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerInterruptEnable, Base::wakeOnMotionInterrupt, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerMotionDetectControl, accelerometerIntelligenceEnable | accelerometerIntelligenceMode, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerWakeOnMotionThreshold, ThresholdRegisterValue(), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerLowPowerAccelerometerOutputDataRate, static_cast<uint8_t>(this->outputDataRate), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->ModifyRegister(Base::registerPowerManagement1, 0, Base::cycleEnable, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Execute([this]()
                    {
                        if (this->dataReadyPinConnected)
                            this->dataReadyPin.EnableInterrupt([this]()
                                {
                                    OnMotionInterrupt();
                                },
                                this->DataReadyTrigger(), hal::InterruptType::dispatched);

                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onWakeOnMotionConfigured();
                            });
                    });
            });
    }

    template<class Base>
    void Mpu9250WithWakeOnMotion<Base>::DisableWakeOnMotion(const infra::Function<void()>& onDone)
    {
        really_assert(this->sequencer.Finished());

        onWakeOnMotionConfigured = onDone;

        if (this->dataReadyPinConnected)
            this->dataReadyPin.DisableInterrupt();

        this->sequencer.Load([this]()
            {
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerPowerManagement1, static_cast<uint8_t>(this->config.clockSource), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerMotionDetectControl, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerInterruptEnable, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerPowerManagement2, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerAccelerometerConfig2, static_cast<uint8_t>(this->config.accelerometerLowPassFilter), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Execute([this]()
                    {
                        onMotion = nullptr;

                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onWakeOnMotionConfigured();
                            });
                    });
            });
    }

    template<class Base>
    void Mpu9250WithWakeOnMotion<Base>::OnMotionInterrupt()
    {
        this->ReadRegister(Base::registerInterruptStatus, infra::MakeByteRange(interruptStatus), [this]()
            {
                if (onMotion)
                    onMotion();
            });
    }

    template<class Base>
    uint8_t Mpu9250WithWakeOnMotion<Base>::ThresholdRegisterValue() const
    {
        return static_cast<uint8_t>(std::min(thresholdMilliG, maximumThresholdMilliG) / thresholdStepMilliG);
    }
}

#endif
