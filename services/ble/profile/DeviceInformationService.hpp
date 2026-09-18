#ifndef SERVICES_DEVICE_INFORMATION_SERVICE_HPP
#define SERVICES_DEVICE_INFORMATION_SERVICE_HPP

#include "services/ble/GattServerCharacteristicImpl.hpp"
#include <optional>

namespace services
{
    struct DeviceInformation
    {
        infra::ConstByteRange manufacturerName;
        infra::ConstByteRange modelNumber;
        infra::ConstByteRange serialNumber;
        infra::ConstByteRange hardwareRevision;
        infra::ConstByteRange firmwareRevision;
        infra::ConstByteRange softwareRevision;
        infra::ConstByteRange systemId;
        infra::ConstByteRange ieeeRegulatoryCertificationDataList;
        infra::ConstByteRange pnpId;
    };

    class DeviceInformationService
    {
    public:
        static constexpr uint16_t maxTextLength = 32;
        static constexpr uint16_t systemIdLength = 8;
        static constexpr uint16_t pnpIdLength = 7;

        DeviceInformationService(GattServer& gattServer, const DeviceInformation& information);

        void SetDeviceInformation(const DeviceInformation& information);

        GattServerService& Service();

    private:
        void AddCharacteristic(std::optional<GattServerCharacteristicImpl>& characteristic, const AttAttribute::Uuid& type, uint16_t valueLength, infra::ConstByteRange value);
        void UpdateCharacteristic(std::optional<GattServerCharacteristicImpl>& characteristic, infra::ConstByteRange value);

    private:
        GattServerService service{ uuid::deviceInformationService };

        std::optional<GattServerCharacteristicImpl> manufacturerName;
        std::optional<GattServerCharacteristicImpl> modelNumber;
        std::optional<GattServerCharacteristicImpl> serialNumber;
        std::optional<GattServerCharacteristicImpl> hardwareRevision;
        std::optional<GattServerCharacteristicImpl> firmwareRevision;
        std::optional<GattServerCharacteristicImpl> softwareRevision;
        std::optional<GattServerCharacteristicImpl> systemId;
        std::optional<GattServerCharacteristicImpl> ieeeRegulatoryCertificationDataList;
        std::optional<GattServerCharacteristicImpl> pnpId;
    };
}

#endif
