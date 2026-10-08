#ifndef DRIVERS_IMU_COMMON_SENSOR_WITH_POLLING_HPP
#define DRIVERS_IMU_COMMON_SENSOR_WITH_POLLING_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    // Mixin template that replaces the data-ready pin with a polling timer.
    // Base must publish protected members registerStatus and dataAvailable.
    template<class Base>
    class SensorWithPolling
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
    void SensorWithPolling<Base>::SetPollingInterval(infra::Duration interval)
    {
        really_assert(interval > infra::Duration::zero());

        pollingInterval = interval;
    }

    template<class Base>
    void SensorWithPolling<Base>::SetVerifyDataReady(bool verify)
    {
        verifyDataReady = verify;
    }

    template<class Base>
    void SensorWithPolling<Base>::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        this->onSampleAvailable = onSampleAvailable;

        pollTimer.Start(pollingInterval, [this]()
            {
                Poll();
            });
    }

    template<class Base>
    void SensorWithPolling<Base>::StopSampling()
    {
        pollTimer.Cancel();
        onSampleAvailable = nullptr;
    }

    template<class Base>
    void SensorWithPolling<Base>::Poll()
    {
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
