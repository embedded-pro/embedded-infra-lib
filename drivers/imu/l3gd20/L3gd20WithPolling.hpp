#ifndef DRIVERS_IMU_L3GD20_L3GD20_WITH_POLLING_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_WITH_POLLING_HPP

#include "drivers/imu/l3gd20/L3gd20Core.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    // Serves a board that leaves the data ready line unwired
    template<class Base>
    class L3gd20WithPolling
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
        uint8_t status = 0;
        bool verifyDataReady = true;
    };

    ////    Implementation    ////

    template<class Base>
    void L3gd20WithPolling<Base>::SetPollingInterval(infra::Duration interval)
    {
        really_assert(interval > infra::Duration::zero());

        pollingInterval = interval;
    }

    template<class Base>
    void L3gd20WithPolling<Base>::SetVerifyDataReady(bool verify)
    {
        verifyDataReady = verify;
    }

    template<class Base>
    void L3gd20WithPolling<Base>::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        this->onSampleAvailable = onSampleAvailable;

        pollTimer.Start(pollingInterval, [this]()
            {
                Poll();
            });
    }

    template<class Base>
    void L3gd20WithPolling<Base>::StopSampling()
    {
        pollTimer.Cancel();
        onSampleAvailable = nullptr;
    }

    template<class Base>
    void L3gd20WithPolling<Base>::Poll()
    {
        // A tick is skipped rather than queued while the device still owes a completion, so a slow
        // bus can never have two transactions in flight
        if (this->TransactionOutstanding())
            return;

        if (!verifyDataReady)
            return onSampleAvailable();

        this->ReadRegister(Base::registerStatus, infra::MakeByteRange(status), [self = this->KeepAlive(*this)]()
            {
                if ((self->status & Base::dataAvailable) != 0)
                    self->onSampleAvailable();
            });
    }
}

#endif
