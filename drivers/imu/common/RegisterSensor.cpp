#include "drivers/imu/common/RegisterSensor.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    RegisterSensor::RegisterSensor(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : bus(bus)
        , dataReadyPin(dataReadyPin)
        , dataReadyPinConnected(&dataReadyPin != &hal::dummyPin)
        , runner(bus, sharedAccess)
    {}

    RegisterSensor::~RegisterSensor()
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();
    }

    void RegisterSensor::Stop(const infra::Function<void()>& onDone)
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();

        StopSampling();
        ClearMeasurementCallbacks();
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

    void RegisterSensor::ReportStopped()
    {
        sharedAccess.SetAction(infra::emptyFunction);

        if (onStopped)
            onStopped();
    }

    bool RegisterSensor::Initialized() const
    {
        return initialized;
    }

    void RegisterSensor::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!onRegisterAccessed);
        onRegisterAccessed = onDone;

        bus.ReadRegister(address, data, [self = KeepAlive(*this)]()
            {
                self->onRegisterAccessed();
            });
    }

    void RegisterSensor::WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone)
    {
        really_assert(!onRegisterAccessed);
        writeValue = value;
        onRegisterAccessed = onDone;

        bus.WriteRegister(address, infra::MakeByteRange(writeValue), [self = KeepAlive(*this)]()
            {
                self->onRegisterAccessed();
            });
    }

    void RegisterSensor::ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone)
    {
        really_assert(!onModified);

        modifyAddress = address;
        modifyClearMask = clearMask;
        modifySetMask = setMask;
        onModified = onDone;

        bus.ReadRegister(modifyAddress, infra::MakeByteRange(modifyValue), [self = KeepAlive(*this)]()
            {
                // Stop() may have been called while the read was in flight; the device must not be
                // written to after it has been stopped
                if (self->stopping)
                    return;

                self->WriteRegister(self->modifyAddress, static_cast<uint8_t>((self->modifyValue & ~self->modifyClearMask) | self->modifySetMask), [self]()
                    {
                        self->onModified();
                    });
            });
    }

    void RegisterSensor::StartSampling(const infra::Function<void()>& onSampleAvailable)
    {
        if (dataReadyPinConnected)
            dataReadyPin.EnableInterrupt(onSampleAvailable, DataReadyTrigger(), hal::InterruptType::dispatched);
    }

    void RegisterSensor::StopSampling()
    {
        if (dataReadyPinConnected)
            dataReadyPin.DisableInterrupt();
    }

    hal::InterruptTrigger RegisterSensor::DataReadyTrigger() const
    {
        return hal::InterruptTrigger::risingEdge;
    }

    void RegisterSensor::UpdateSampling(bool wanted)
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

    bool RegisterSensor::Sampling() const
    {
        return sampling;
    }

    bool RegisterSensor::TransactionOutstanding() const
    {
        return static_cast<bool>(onRegisterAccessed) || static_cast<bool>(onModified) || runner.Busy();
    }
}
