#ifndef SERVICES_GAP_PERIPHERAL_HPP
#define SERVICES_GAP_PERIPHERAL_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    class GapPeripheral;

    class GapPeripheralObserver
        : public infra::Observer<GapPeripheralObserver, GapPeripheral>
    {
    public:
        using infra::Observer<GapPeripheralObserver, GapPeripheral>::Observer;

        virtual void StateChanged(GapState state) = 0;
    };

    class GapPeripheral
        : public infra::Subject<GapPeripheralObserver>
    {
    public:
        using AdvertisementIntervalMultiplier = uint16_t;                                              // Interval = Multiplier * 0.625 ms.
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMin = 0x20u;   // 20 ms
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMax = 0x4000u; // 10240 ms

        enum class Result : uint8_t
        {
            success = 0,
            invalidParameter,
            controllerError
        };

    public:
        virtual GapAddress GetAddress() const = 0;
        virtual GapAddress GetIdentityAddress() const = 0;
        virtual infra::ConstByteRange GetAdvertisementData() const = 0;
        virtual infra::ConstByteRange GetScanResponseData() const = 0;

        // Each procedure below reports whether the request is accepted through its
        // return value, and its outcome through onDone. onDone is never invoked from
        // within the call itself; it is scheduled on the event dispatcher. A request
        // that is not accepted never results in a call to onDone.
        virtual GapRequestStatus SetAdvertisementData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetScanResponseData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Standby(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetConnectionParameters(const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone) = 0;
    };

    class GapPeripheralDecorator
        : public GapPeripheralObserver
        , public GapPeripheral
    {
    public:
        using GapPeripheralObserver::GapPeripheralObserver;

        // Implementation of GapPeripheralObserver
        void StateChanged(GapState state) override;

        // Implementation of GapPeripheral
        GapAddress GetAddress() const override;
        GapAddress GetIdentityAddress() const override;
        infra::ConstByteRange GetAdvertisementData() const override;
        infra::ConstByteRange GetScanResponseData() const override;
        GapRequestStatus SetAdvertisementData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetScanResponseData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Standby(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetConnectionParameters(const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone) override;
    };
}

#endif
