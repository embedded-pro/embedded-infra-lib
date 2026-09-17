#ifndef DRIVERS_IMU_MPU9250_MPU9250_WITH_POLLING_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_WITH_POLLING_HPP

#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    template<class Base>
    class Mpu9250WithPolling
        : public Base
    {
    public:
        using Base::Base;

        void SetPollingInterval(infra::Duration interval);
        void SetVerifyDataReady(bool verify);

    protected:
        void StartSampling(const infra::Function<void()>& onSampleAvailable) override;
        void StopSampling() override;

    private:
        void Poll();

        infra::TimerRepeating pollTimer;
        infra::Duration pollingInterval{ std::chrono::milliseconds(5) };
        infra::Function<void()> onSampleAvailable;
        uint8_t interruptStatus = 0;
        bool verifyDataReady = true;
    };

    ////    Implementation    ////

    template<class Base>
    void Mpu9250WithPolling<Base>::SetPollingInterval(infra::Duration interval)
    {
        really_assert(interval > infra::Duration::zero());

        pollingInterval = interval;
    }

    template<class Base>
    void Mpu9250WithPolling<Base>::SetVerifyDataReady(bool verify)
    {
        verifyDataReady = verify;
    }

    template<class Base>
    void Mpu9250WithPolling<Base>::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        this->onSampleAvailable = onSampleAvailable;

        pollTimer.Start(pollingInterval, [this]()
            {
                Poll();
            });
    }

    template<class Base>
    void Mpu9250WithPolling<Base>::StopSampling()
    {
        pollTimer.Cancel();
        onSampleAvailable = nullptr;
    }

    template<class Base>
    void Mpu9250WithPolling<Base>::Poll()
    {
        // A tick is skipped rather than queued while the device still owes a completion, so a slow
        // bus can never have two transactions in flight
        if (this->TransactionOutstanding())
            return;

        if (!verifyDataReady)
            return onSampleAvailable();

        this->ReadRegister(Base::registerInterruptStatus, infra::MakeByteRange(interruptStatus), [self = this->KeepAlive(*this)]()
            {
                if ((self->interruptStatus & Base::rawDataReadyInterrupt) != 0)
                    self->onSampleAvailable();
            });
    }
}

#endif
