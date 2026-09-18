#include "services/ble/profile/BatteryService.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    BatteryService::BatteryService(GattServer& gattServer)
    {
        gattServer.AddService(service);
    }

    void BatteryService::BatteryLevelChanged(uint8_t level)
    {
        really_assert(level <= maxBatteryLevel);

        batteryLevel = level;

        batteryLevelCharacteristic.Update(infra::MakeConstByteRange(batteryLevel), []() {});
    }

    uint8_t BatteryService::BatteryLevel() const
    {
        return batteryLevel;
    }

    GattServerService& BatteryService::Service()
    {
        return service;
    }
}
