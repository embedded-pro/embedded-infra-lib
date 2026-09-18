#include "services/ble/profile/DeviceInformationService.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    DeviceInformationService::DeviceInformationService(GattServer& gattServer, const DeviceInformation& information)
    {
        AddCharacteristic(manufacturerName, uuid::manufacturerName, maxTextLength, information.manufacturerName);
        AddCharacteristic(modelNumber, uuid::modelNumber, maxTextLength, information.modelNumber);
        AddCharacteristic(serialNumber, uuid::serialNumber, maxTextLength, information.serialNumber);
        AddCharacteristic(hardwareRevision, uuid::hardwareRevision, maxTextLength, information.hardwareRevision);
        AddCharacteristic(firmwareRevision, uuid::firmwareRevision, maxTextLength, information.firmwareRevision);
        AddCharacteristic(softwareRevision, uuid::softwareRevision, maxTextLength, information.softwareRevision);
        AddCharacteristic(systemId, uuid::systemId, systemIdLength, information.systemId);
        AddCharacteristic(ieeeRegulatoryCertificationDataList, uuid::ieeeCertification, maxTextLength, information.ieeeRegulatoryCertificationDataList);
        AddCharacteristic(pnpId, uuid::pnpId, pnpIdLength, information.pnpId);

        gattServer.AddService(service);
    }

    void DeviceInformationService::SetDeviceInformation(const DeviceInformation& information)
    {
        UpdateCharacteristic(manufacturerName, information.manufacturerName);
        UpdateCharacteristic(modelNumber, information.modelNumber);
        UpdateCharacteristic(serialNumber, information.serialNumber);
        UpdateCharacteristic(hardwareRevision, information.hardwareRevision);
        UpdateCharacteristic(firmwareRevision, information.firmwareRevision);
        UpdateCharacteristic(softwareRevision, information.softwareRevision);
        UpdateCharacteristic(systemId, information.systemId);
        UpdateCharacteristic(ieeeRegulatoryCertificationDataList, information.ieeeRegulatoryCertificationDataList);
        UpdateCharacteristic(pnpId, information.pnpId);
    }

    GattServerService& DeviceInformationService::Service()
    {
        return service;
    }

    void DeviceInformationService::AddCharacteristic(std::optional<GattServerCharacteristicImpl>& characteristic, const AttAttribute::Uuid& type, uint16_t valueLength, infra::ConstByteRange value)
    {
        if (value.empty())
            return;

        really_assert(value.size() <= valueLength);

        characteristic.emplace(service, type, valueLength, GattCharacteristic::PropertyFlags::read);
    }

    void DeviceInformationService::UpdateCharacteristic(std::optional<GattServerCharacteristicImpl>& characteristic, infra::ConstByteRange value)
    {
        really_assert(characteristic || value.empty());

        if (!characteristic || value.empty())
            return;

        really_assert(value.size() <= characteristic->ValueLength());

        characteristic->Update(value, []() {});
    }
}
