#ifndef SERVICES_GAP_PERIPHERAL_HPP
#define SERVICES_GAP_PERIPHERAL_HPP

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

    public:
        virtual GapAddress GetAddress() const = 0;
        virtual GapAddress GetIdentityAddress() const = 0;
        virtual void SetAdvertisementData(infra::ConstByteRange data) = 0;
        virtual infra::ConstByteRange GetAdvertisementData() const = 0;
        virtual void SetScanResponseData(infra::ConstByteRange data) = 0;
        virtual infra::ConstByteRange GetScanResponseData() const = 0;
        virtual void Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier) = 0;
        virtual void Standby() = 0;
        virtual void SetConnectionParameters(const GapConnectionParameters& connParam) = 0;
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
        void SetAdvertisementData(infra::ConstByteRange data) override;
        infra::ConstByteRange GetAdvertisementData() const override;
        void SetScanResponseData(infra::ConstByteRange data) override;
        infra::ConstByteRange GetScanResponseData() const override;
        void Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier) override;
        void Standby() override;
        void SetConnectionParameters(const GapConnectionParameters& connParam) override;
    };
}

#endif
