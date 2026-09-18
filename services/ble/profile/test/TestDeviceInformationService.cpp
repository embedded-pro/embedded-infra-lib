#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/ble/profile/DeviceInformationService.hpp"
#include "services/ble/test_doubles/GattServerMock.hpp"
#include "gmock/gmock.h"
#include <algorithm>
#include <optional>

namespace
{
    class DeviceInformationServiceTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        void Create(const services::DeviceInformation& information)
        {
            EXPECT_CALL(gattServer, AddService(testing::_));
            deviceInformationService.emplace(gattServer, information);

            for (auto& characteristic : deviceInformationService->Service().Characteristics())
                characteristic.Attach(operations);
        }

        std::size_t CharacteristicCount()
        {
            auto& characteristics = deviceInformationService->Service().Characteristics();
            return std::distance(characteristics.begin(), characteristics.end());
        }

        bool Exposes(const services::AttAttribute::Uuid& type)
        {
            auto& characteristics = deviceInformationService->Service().Characteristics();
            return std::find_if(characteristics.begin(), characteristics.end(), [&type](const auto& characteristic)
                       {
                           return characteristic.Type() == type;
                       }) != characteristics.end();
        }

        services::GattServerCharacteristic& Characteristic(const services::AttAttribute::Uuid& type)
        {
            auto& characteristics = deviceInformationService->Service().Characteristics();
            auto found = std::find_if(characteristics.begin(), characteristics.end(), [&type](const auto& characteristic)
                {
                    return characteristic.Type() == type;
                });

            really_assert(found != characteristics.end());
            return *found;
        }

        testing::StrictMock<services::GattServerMock> gattServer;
        testing::StrictMock<services::GattServerCharacteristicOperationsMock> operations;
        std::optional<services::DeviceInformationService> deviceInformationService;

        const std::array<uint8_t, 6> manufacturer{ 'A', 'c', 'm', 'e', ' ', 'B' };
        const std::array<uint8_t, 4> firmware{ '1', '.', '0', '2' };
        const std::array<uint8_t, 8> system{ 0x01, 0x02, 0x03, 0xFF, 0xFE, 0x04, 0x05, 0x06 };
    };
}

TEST_F(DeviceInformationServiceTest, exposes_only_the_fields_that_were_given)
{
    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer), .firmwareRevision = infra::MakeConstByteRange(firmware) });

    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::deviceInformationService }, deviceInformationService->Service().Type());
    EXPECT_EQ(2, CharacteristicCount());
    EXPECT_TRUE(Exposes(services::uuid::manufacturerName));
    EXPECT_TRUE(Exposes(services::uuid::firmwareRevision));
    EXPECT_FALSE(Exposes(services::uuid::serialNumber));
    EXPECT_FALSE(Exposes(services::uuid::pnpId));
}

TEST_F(DeviceInformationServiceTest, an_empty_device_information_exposes_a_bare_service)
{
    Create({});

    EXPECT_EQ(0, CharacteristicCount());
}

TEST_F(DeviceInformationServiceTest, every_field_of_the_specification_is_available)
{
    const std::array<uint8_t, 7> pnp{ 0x01, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x00 };

    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer),
        .modelNumber = infra::MakeConstByteRange(manufacturer),
        .serialNumber = infra::MakeConstByteRange(manufacturer),
        .hardwareRevision = infra::MakeConstByteRange(firmware),
        .firmwareRevision = infra::MakeConstByteRange(firmware),
        .softwareRevision = infra::MakeConstByteRange(firmware),
        .systemId = infra::MakeConstByteRange(system),
        .ieeeRegulatoryCertificationDataList = infra::MakeConstByteRange(firmware),
        .pnpId = infra::MakeConstByteRange(pnp) });

    EXPECT_EQ(9, CharacteristicCount());
}

TEST_F(DeviceInformationServiceTest, characteristics_are_read_only)
{
    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer) });

    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::read, Characteristic(services::uuid::manufacturerName).Properties());
}

TEST_F(DeviceInformationServiceTest, a_text_field_is_declared_at_the_length_the_echo_interface_bounds_it_to)
{
    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer), .systemId = infra::MakeConstByteRange(system) });

    EXPECT_EQ(services::DeviceInformationService::maxTextLength, Characteristic(services::uuid::manufacturerName).ValueLength());
    EXPECT_EQ(services::DeviceInformationService::systemIdLength, Characteristic(services::uuid::systemId).ValueLength());
}

TEST_F(DeviceInformationServiceTest, setting_the_information_updates_every_exposed_characteristic)
{
    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer), .firmwareRevision = infra::MakeConstByteRange(firmware) });

    const std::array<uint8_t, 4> newFirmware{ '2', '.', '0', '0' };

    EXPECT_CALL(operations, Update(testing::Ref(Characteristic(services::uuid::manufacturerName)), testing::ElementsAreArray(manufacturer))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_CALL(operations, Update(testing::Ref(Characteristic(services::uuid::firmwareRevision)), testing::ElementsAreArray(newFirmware))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    deviceInformationService->SetDeviceInformation({ .manufacturerName = infra::MakeConstByteRange(manufacturer), .firmwareRevision = infra::MakeConstByteRange(newFirmware) });
}

TEST_F(DeviceInformationServiceTest, a_field_left_out_of_a_later_set_keeps_its_value)
{
    Create({ .manufacturerName = infra::MakeConstByteRange(manufacturer), .firmwareRevision = infra::MakeConstByteRange(firmware) });

    EXPECT_CALL(operations, Update(testing::Ref(Characteristic(services::uuid::firmwareRevision)), testing::ElementsAreArray(firmware))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    deviceInformationService->SetDeviceInformation({ .firmwareRevision = infra::MakeConstByteRange(firmware) });
}
