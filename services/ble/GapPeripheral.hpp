#ifndef SERVICES_GAP_PERIPHERAL_HPP
#define SERVICES_GAP_PERIPHERAL_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    enum class GapPeripheralState : uint8_t
    {
        standby,
        advertising,
        connected
    };

    class GapPeripheral;

    class GapPeripheralObserver
        : public infra::Observer<GapPeripheralObserver, GapPeripheral>
    {
    public:
        using infra::Observer<GapPeripheralObserver, GapPeripheral>::Observer;

        virtual void StateChanged(GapPeripheralState state) = 0;
    };

    class GapPeripheral
        : public infra::Subject<GapPeripheralObserver>
    {
    public:
        using AdvertisementIntervalMultiplier = GapAdvertisingParameters::IntervalMultiplier;
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMin = GapAdvertisingParameters::intervalMultiplierMin;
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMax = GapAdvertisingParameters::intervalMultiplierMax;

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

        virtual GapRequestStatus SetAdvertisementData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetScanResponseData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) = 0;
        // All three primary channels, taking scan and connection requests from anyone: what a
        // peripheral that has not been told otherwise should do.
        static constexpr GapAdvertisingParameters defaultAdvertisingParameters{ GapAdvertisementType::advInd, 0x0080u, GapAdvertisingChannels::all, GapAdvertisingFilterPolicy::any };

        virtual GapRequestStatus Advertise(const GapAdvertisingParameters& parameters, const infra::Function<void(Result)>& onDone) = 0;
        GapRequestStatus Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone);

        // Directed advertising names the peer it is aimed at. High duty cycle ignores the
        // interval, which the controller fixes.
        virtual GapRequestStatus AdvertiseDirected(GapDirectedAdvertisementType type, const GapAddress& peer, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Standby(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus RequestConnectionParameterUpdate(const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone) = 0;
    };

    class GapPeripheralDecorator
        : public GapPeripheralObserver
        , public GapPeripheral
    {
    public:
        using GapPeripheralObserver::GapPeripheralObserver;

        // Implementation of GapPeripheralObserver
        void StateChanged(GapPeripheralState state) override;

        // Implementation of GapPeripheral
        GapAddress GetAddress() const override;
        GapAddress GetIdentityAddress() const override;
        infra::ConstByteRange GetAdvertisementData() const override;
        infra::ConstByteRange GetScanResponseData() const override;
        GapRequestStatus SetAdvertisementData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetScanResponseData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone) override;
        using GapPeripheral::Advertise;
        GapRequestStatus Advertise(const GapAdvertisingParameters& parameters, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus AdvertiseDirected(GapDirectedAdvertisementType type, const GapAddress& peer, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Standby(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus RequestConnectionParameterUpdate(const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone) override;
    };
}

namespace infra
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const services::GapPeripheralState& state);
}

#endif
