#ifndef DRIVERS_IMU_L3GD20_L3GD20_WITH_THRESHOLD_INTERRUPT_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_WITH_THRESHOLD_INTERRUPT_HPP

#include "drivers/imu/l3gd20/L3gd20Core.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    // Raises INT1 when the angular rate on a selected axis crosses its threshold, which is how a
    // sleeping application is woken by motion. The data ready and buffer sources stay on INT2, so
    // every touch of CTRL_REG3 leaves their bits alone.
    template<class Base>
    class L3gd20WithThresholdInterrupt
        : public Base
    {
    public:
        using Base::Base;

        struct ThresholdConfig
        {
            // Fifteen bit magnitudes in counts at the configured full scale
            std::array<uint16_t, 3> threshold = {};

            bool highX = true;
            bool lowX = false;
            bool highY = true;
            bool lowY = false;
            bool highZ = true;
            bool lowZ = false;

            bool combineWithAnd = false;
            bool latch = true;
            uint8_t duration = 0;
            bool waitBeforeRelease = false;
            bool useHighPassFilter = false;

            // L3GD20H only; counts the duration down instead of resetting it
            bool decrementMode = false;
        };

        struct ThresholdEvent
        {
            bool highX = false;
            bool lowX = false;
            bool highY = false;
            bool lowY = false;
            bool highZ = false;
            bool lowZ = false;
        };

        void EnableThresholdInterrupt(const ThresholdConfig& thresholdConfig, const infra::Function<void(ThresholdEvent)>& onThreshold, const infra::Function<void()>& onDone);
        void DisableThresholdInterrupt(const infra::Function<void()>& onDone);

    protected:
        void ClearCallbacks() override;

    private:
        void ArmInterruptPin();
        void OnThresholdInterrupt();
        void ReportThreshold();
        void FillThresholdBuffer();
        uint8_t ConfigurationValue() const;
        uint8_t DurationValue() const;
        uint8_t Control5Value() const;

        static constexpr uint8_t combineWithAnd = 0x80;
        static constexpr uint8_t latchInterrupt = 0x40;
        static constexpr uint8_t enableHighZ = 0x20;
        static constexpr uint8_t enableLowZ = 0x10;
        static constexpr uint8_t enableHighY = 0x08;
        static constexpr uint8_t enableLowY = 0x04;
        static constexpr uint8_t enableHighX = 0x02;
        static constexpr uint8_t enableLowX = 0x01;

        static constexpr uint8_t interruptActive = 0x40;
        static constexpr uint8_t waitBeforeRelease = 0x80;
        static constexpr uint8_t durationMask = 0x7f;
        static constexpr uint8_t decrementMode = 0x80;
        static constexpr uint8_t thresholdHighMask = 0x7f;

        static constexpr uint8_t interruptSelectMask = 0x0c;
        static constexpr uint8_t interruptSelectHighPass = 0x04;

        static constexpr std::size_t thresholdSize = 6;

        ThresholdConfig thresholdConfig;
        infra::Function<void(ThresholdEvent)> onThreshold;
        infra::AutoResetFunction<void()> onThresholdConfigured;

        std::array<uint8_t, thresholdSize> thresholdBuffer = {};
        uint8_t source = 0;
        bool enabled = false;
    };

    ////    Implementation    ////

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::EnableThresholdInterrupt(const ThresholdConfig& thresholdConfig, const infra::Function<void(ThresholdEvent)>& onThreshold, const infra::Function<void()>& onDone)
    {
        really_assert(this->Initialized());
        really_assert(!this->runner.Busy());
        really_assert(this->HasLowOutputDataRateRegister() || !thresholdConfig.decrementMode);

        this->thresholdConfig = thresholdConfig;
        this->onThreshold = onThreshold;
        onThresholdConfigured = onDone;

        FillThresholdBuffer();

        this->runner.Clear();
        // Disarmed while the thresholds are in flight, so a half written value cannot raise the line
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, Base::interrupt1Enable, 0 });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerInterrupt1Configuration, 0 });
        this->runner.Push(services::RegisterStepRunner::WriteBurst{ Base::registerInterrupt1ThresholdXHigh, infra::MakeRange(thresholdBuffer) });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerInterrupt1Duration, DurationValue() });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl5, static_cast<uint8_t>(Base::highPassEnable | interruptSelectMask), Control5Value() });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerInterrupt1Configuration, ConfigurationValue() });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, Base::interrupt1Enable, Base::interrupt1Enable });

        this->runner.Start([this]()
            {
                enabled = true;
                ArmInterruptPin();
                onThresholdConfigured();
            });
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::DisableThresholdInterrupt(const infra::Function<void()>& onDone)
    {
        really_assert(!this->runner.Busy());

        if (this->interruptPinConnected)
            this->interruptPin.DisableInterrupt();

        onThresholdConfigured = onDone;

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, Base::interrupt1Enable, 0 });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerInterrupt1Configuration, 0 });

        this->runner.Start([this]()
            {
                enabled = false;
                onThreshold = nullptr;
                onThresholdConfigured();
            });
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::ClearCallbacks()
    {
        onThreshold = nullptr;

        Base::ClearCallbacks();
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::ArmInterruptPin()
    {
        if (!this->interruptPinConnected)
            return;

        this->interruptPin.EnableInterrupt([self = this->KeepAlive(*this)]()
            {
                self->OnThresholdInterrupt();
            },
            this->ThresholdInterruptTrigger(), hal::InterruptType::dispatched);
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::OnThresholdInterrupt()
    {
        // Reading the source register is also what releases a latched interrupt
        this->ReadRegister(Base::registerInterrupt1Source, infra::MakeByteRange(source), [self = this->KeepAlive(*this)]()
            {
                self->ReportThreshold();
            });
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::ReportThreshold()
    {
        if ((source & interruptActive) == 0 || !onThreshold)
            return;

        onThreshold(ThresholdEvent{ (source & enableHighX) != 0, (source & enableLowX) != 0,
            (source & enableHighY) != 0, (source & enableLowY) != 0,
            (source & enableHighZ) != 0, (source & enableLowZ) != 0 });
    }

    template<class Base>
    void L3gd20WithThresholdInterrupt<Base>::FillThresholdBuffer()
    {
        // High byte first, the opposite of the output registers
        for (std::size_t axis = 0; axis != thresholdConfig.threshold.size(); ++axis)
        {
            thresholdBuffer[2 * axis] = static_cast<uint8_t>((thresholdConfig.threshold[axis] >> 8) & thresholdHighMask);
            thresholdBuffer[2 * axis + 1] = static_cast<uint8_t>(thresholdConfig.threshold[axis] & 0xff);
        }

        if (thresholdConfig.decrementMode)
            thresholdBuffer[0] |= decrementMode;
    }

    template<class Base>
    uint8_t L3gd20WithThresholdInterrupt<Base>::ConfigurationValue() const
    {
        return static_cast<uint8_t>((thresholdConfig.combineWithAnd ? combineWithAnd : 0) | (thresholdConfig.latch ? latchInterrupt : 0) | (thresholdConfig.highZ ? enableHighZ : 0) | (thresholdConfig.lowZ ? enableLowZ : 0) | (thresholdConfig.highY ? enableHighY : 0) | (thresholdConfig.lowY ? enableLowY : 0) | (thresholdConfig.highX ? enableHighX : 0) | (thresholdConfig.lowX ? enableLowX : 0));
    }

    template<class Base>
    uint8_t L3gd20WithThresholdInterrupt<Base>::DurationValue() const
    {
        return static_cast<uint8_t>((thresholdConfig.waitBeforeRelease ? waitBeforeRelease : 0) | (thresholdConfig.duration & durationMask));
    }

    template<class Base>
    uint8_t L3gd20WithThresholdInterrupt<Base>::Control5Value() const
    {
        return thresholdConfig.useHighPassFilter ? static_cast<uint8_t>(Base::highPassEnable | interruptSelectHighPass) : 0;
    }
}

#endif
