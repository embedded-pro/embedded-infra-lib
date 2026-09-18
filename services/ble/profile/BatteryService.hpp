#ifndef SERVICES_BATTERY_SERVICE_HPP
#define SERVICES_BATTERY_SERVICE_HPP

#include "services/ble/GattServerCharacteristicImpl.hpp"

namespace services
{
    class BatteryService
    {
    public:
        static constexpr uint8_t maxBatteryLevel = 100;

        explicit BatteryService(GattServer& gattServer);

        void BatteryLevelChanged(uint8_t level);
        uint8_t BatteryLevel() const;

        GattServerService& Service();

    private:
        GattServerService service{ uuid::batteryService };
        GattServerCharacteristicImpl batteryLevelCharacteristic{ service, uuid::batteryLevel, sizeof(uint8_t), GattCharacteristic::PropertyFlags::read | GattCharacteristic::PropertyFlags::notify };
        uint8_t batteryLevel{ 0 };
    };
}

#endif
