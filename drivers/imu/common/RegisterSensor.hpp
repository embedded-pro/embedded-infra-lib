#ifndef DRIVERS_IMU_COMMON_REGISTER_SENSOR_HPP
#define DRIVERS_IMU_COMMON_REGISTER_SENSOR_HPP

#include "hal/interfaces/Gpio.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include "services/util/RegisterStepRunner.hpp"
#include "services/util/Stoppable.hpp"
#include <cstdint>

namespace drivers
{
    // Plumbing shared by register-based IMU sensors that have an independent bus.
    // One bus transaction is outstanding at a time, so Initialize, the setters, Start and Stop must
    // not be invoked while a previous one is still running. Completions are delivered from the event
    // dispatcher, so calling them from a completion callback is safe.
    // Call Stop() and destroy only from its callback: an outstanding bus transaction holds a
    // reference, and destroying while referenced trips the assertion in ~AccessedBySharedPtr.
    class RegisterSensor
        : public services::Stoppable
    {
    public:
        using Action = infra::Function<void(), sizeof(void*)>;

        enum class InitializationResult : uint8_t
        {
            success,
            deviceNotFound
        };

        RegisterSensor(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin);
        RegisterSensor(const RegisterSensor& other) = delete;
        RegisterSensor& operator=(const RegisterSensor& other) = delete;

        void Stop(const infra::Function<void()>& onDone) override;

        bool Initialized() const;

    protected:
        ~RegisterSensor();

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone);
        void WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone);
        void ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone);

        virtual void StartSampling(const infra::Function<void()>& onSampleAvailable);
        virtual void StopSampling();
        virtual void ReadAndDeliverSamples() = 0;
        virtual void EnableDataReadyInterrupt(bool enable) = 0;
        virtual void ClearMeasurementCallbacks() = 0;
        virtual hal::InterruptTrigger DataReadyTrigger() const;

        void UpdateSampling(bool wanted);
        bool Sampling() const;
        bool TransactionOutstanding() const;

        template<class T>
        infra::SharedPtr<T> KeepAlive(T& object)
        {
            return sharedAccess.MakeShared(object);
        }

        services::RegisterBusAccess& bus;
        hal::InputPin dataReadyPin;
        bool dataReadyPinConnected;
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        services::RegisterStepRunner runner;
        bool initialized = false;
        bool stopping = false;

    private:
        void ReportStopped();

        infra::AutoResetFunction<void()> onRegisterAccessed;
        infra::AutoResetFunction<void()> onModified;
        infra::AutoResetFunction<void()> onStopped;

        uint8_t writeValue = 0;
        uint8_t modifyValue = 0;
        uint8_t modifyAddress = 0;
        uint8_t modifyClearMask = 0;
        uint8_t modifySetMask = 0;
        bool sampling = false;
    };
}

#endif
