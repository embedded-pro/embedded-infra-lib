#ifndef DRIVERS_IMU_MPU9250_MPU9250_WITH_POLLING_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_WITH_POLLING_HPP

#include "drivers/imu/mpu9250/Mpu9250Core.hpp"

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
        bool reading = false;
    };

    ////    Implementation    ////

    template<class Base>
    void Mpu9250WithPolling<Base>::SetPollingInterval(infra::Duration interval)
    {
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
        reading = false;
    }

    template<class Base>
    void Mpu9250WithPolling<Base>::Poll()
    {
        if (reading)
            return;

        if (!verifyDataReady)
        {
            onSampleAvailable();
            return;
        }

        reading = true;

        this->ReadRegister(Base::registerInterruptStatus, infra::MakeByteRange(interruptStatus), [this]()
            {
                reading = false;

                if ((interruptStatus & Base::rawDataReadyInterrupt) != 0)
                    onSampleAvailable();
            });
    }
}

#endif
