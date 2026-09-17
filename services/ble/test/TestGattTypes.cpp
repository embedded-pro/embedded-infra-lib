#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/ble/GattTypes.hpp"
#include "gmock/gmock.h"
#include <utility>

namespace
{
    services::AttAttribute::Uuid16 uuid16{ 0x42 };
}

TEST(GattDescriptor, access)
{
    uint16_t type = 1;
    services::AttAttribute::Handle handle = 2;
    services::GattDescriptor descriptor{ type, handle };

    EXPECT_EQ(services::AttAttribute::Uuid(std::in_place_type_t<uint16_t>(), type), descriptor.Type());
    EXPECT_EQ(handle, descriptor.Handle());
    EXPECT_EQ(handle, const_cast<const services::GattDescriptor&>(descriptor).Handle());
}

TEST(GattDescriptor, equality)
{
    uint16_t type = 1;
    services::GattDescriptor descriptor1{ type, 2 };
    services::GattDescriptor descriptor2{ type, 3 };

    EXPECT_TRUE(descriptor1 == descriptor1);
    EXPECT_TRUE(descriptor1 != descriptor2);
}

TEST(GattTest, service_has_handle_and_type)
{
    services::GattService s{ uuid16 };

    EXPECT_EQ(0x42, std::get<services::AttAttribute::Uuid16>(s.Type()));
    EXPECT_EQ(0, s.Handle());
    EXPECT_EQ(0, s.EndHandle());
    EXPECT_EQ(0, const_cast<const services::GattService&>(s).EndHandle());
    EXPECT_EQ(0, s.GetAttributeCount());
}

TEST(GattTest, service_handle_is_updated)
{
    services::GattService s{ uuid16 };
    s.Handle() = 0xAB;

    EXPECT_EQ(0xAB, s.Handle());
    EXPECT_EQ(0xAB, std::as_const(s).Handle());
}

TEST(GattTest, characteristic_handles_are_accesible)
{
    services::GattCharacteristic c;

    c.Handle() = 0xCD;
    c.ValueHandle() = 0xFE;

    EXPECT_EQ(0xCD, c.Handle());
    EXPECT_EQ(0xFE, c.ValueHandle());
}

TEST(GattTest, const_characteristic)
{
    const services::GattCharacteristic c{ uuid16, 0xCD, 0xFE, services::GattCharacteristic::PropertyFlags::none };

    EXPECT_EQ(0xCD, c.Handle());
    EXPECT_EQ(0xFE, c.ValueHandle());
    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::none, c.Properties());
}

TEST(GattInsertionOperatorPropertyFlagsTest, property_flags_overload_operator)
{
    infra::StringOutputStream::WithStorage<128> stream;

    services::GattCharacteristic::PropertyFlags properties = services::GattCharacteristic::PropertyFlags::broadcast |
                                                             services::GattCharacteristic::PropertyFlags::read |
                                                             services::GattCharacteristic::PropertyFlags::writeWithoutResponse |
                                                             services::GattCharacteristic::PropertyFlags::write |
                                                             services::GattCharacteristic::PropertyFlags::notify |
                                                             services::GattCharacteristic::PropertyFlags::indicate |
                                                             services::GattCharacteristic::PropertyFlags::signedWrite |
                                                             services::GattCharacteristic::PropertyFlags::extended;

    stream << properties;

    EXPECT_EQ("[|broadcast||read||writeWithoutResponse||write||notify||indicate||signedWrite||extended|]", stream.Storage());
}

TEST(GattInsertionOperatorPropertyFlagsTest, property_flags_overload_operator_flag_none)
{
    infra::StringOutputStream::WithStorage<128> stream;

    services::GattCharacteristic::PropertyFlags properties = services::GattCharacteristic::PropertyFlags::none;

    stream << properties;

    EXPECT_EQ("[]", stream.Storage());
}

TEST(GattInsertionOperatorUuidTest, uuid_overload_operator)
{
    infra::StringOutputStream::WithStorage<128> stream;

    services::AttAttribute::Uuid128 uuid128{ { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 } };

    stream << "Uuid16: " << services::AttAttribute::Uuid(uuid16) << ", Uuid128: " << services::AttAttribute::Uuid(uuid128);

    EXPECT_EQ("Uuid16: [42], Uuid128: [100f0e0d0c0b0a090807060504030201]", stream.Storage());
}

TEST(GattResultFromAttErrorCodeTest, maps_every_specified_error_code)
{
    using services::AttErrorCode;
    using services::GattResult;

    const std::pair<AttErrorCode, GattResult> expectations[]{
        { AttErrorCode::success, GattResult::success },
        { AttErrorCode::invalidHandle, GattResult::invalidHandle },
        { AttErrorCode::readNotPermitted, GattResult::notPermitted },
        { AttErrorCode::writeNotPermitted, GattResult::notPermitted },
        { AttErrorCode::invalidPdu, GattResult::unknown },
        { AttErrorCode::insufficientAuthentication, GattResult::insufficientAuthentication },
        { AttErrorCode::requestNotSupported, GattResult::unsupported },
        { AttErrorCode::invalidOffset, GattResult::invalidLength },
        { AttErrorCode::insufficientAuthorization, GattResult::insufficientAuthorization },
        { AttErrorCode::prepareQueueFull, GattResult::insufficientResources },
        { AttErrorCode::attributeNotFound, GattResult::invalidHandle },
        { AttErrorCode::attributeNotLong, GattResult::invalidLength },
        { AttErrorCode::insufficientEncryptionKeySize, GattResult::insufficientEncryption },
        { AttErrorCode::invalidAttributeValueLength, GattResult::invalidLength },
        { AttErrorCode::unlikelyError, GattResult::unknown },
        { AttErrorCode::insufficientEncryption, GattResult::insufficientEncryption },
        { AttErrorCode::unsupportedGroupType, GattResult::unsupported },
        { AttErrorCode::insufficientResources, GattResult::insufficientResources },
        { AttErrorCode::databaseOutOfSync, GattResult::databaseOutOfSync },
        { AttErrorCode::valueNotAllowed, GattResult::valueNotAllowed },
    };

    for (const auto& [errorCode, result] : expectations)
        EXPECT_EQ(result, services::GattResultFromAttErrorCode(infra::enum_cast(errorCode))) << "for ATT error code " << infra::enum_cast(errorCode);
}

TEST(GattResultFromAttErrorCodeTest, maps_application_error_codes_to_unknown)
{
    EXPECT_EQ(services::GattResult::unknown, services::GattResultFromAttErrorCode(0x80));
    EXPECT_EQ(services::GattResult::unknown, services::GattResultFromAttErrorCode(0xff));
}

TEST(GattUuidTest, defines_the_service_declaration_uuids)
{
    EXPECT_EQ(0x2800, services::uuid::primaryService);
    EXPECT_EQ(0x2801, services::uuid::secondaryService);
    EXPECT_EQ(0x2802, services::uuid::include);
    EXPECT_EQ(0x2803, services::uuid::characteristic);
}

TEST(GattUuidTest, defines_the_core_descriptor_uuids)
{
    EXPECT_EQ(0x2900, services::uuid::characteristicExtendedProperties);
    EXPECT_EQ(0x2901, services::uuid::characteristicUserDescription);
    EXPECT_EQ(0x2902, services::uuid::clientCharacteristicConfiguration);
    EXPECT_EQ(0x2903, services::uuid::serverCharacteristicConfiguration);
    EXPECT_EQ(0x2904, services::uuid::characteristicPresentationFormat);
    EXPECT_EQ(0x2905, services::uuid::characteristicAggregateFormat);
}

TEST(GattUuidTest, defines_the_generic_access_and_attribute_services)
{
    EXPECT_EQ(0x1800, services::uuid::genericAccessService);
    EXPECT_EQ(0x1801, services::uuid::genericAttributeService);
}

TEST(GattUuidTest, defines_the_cache_coherence_characteristics)
{
    EXPECT_EQ(0x2A05, services::uuid::serviceChanged);
    EXPECT_EQ(0x2B29, services::uuid::clientSupportedFeatures);
    EXPECT_EQ(0x2B2A, services::uuid::databaseHash);
    EXPECT_EQ(0x2B3A, services::uuid::serverSupportedFeatures);
}

TEST(GattUuidTest, the_client_characteristic_configuration_attribute_type_is_the_declared_uuid)
{
    EXPECT_EQ(services::uuid::clientCharacteristicConfiguration,
        services::GattDescriptor::ClientCharacteristicConfiguration::attributeType);
}

TEST(GattCharacteristicExtendedPropertiesTest, combines_and_masks_its_bits)
{
    auto both = services::GattCharacteristicExtendedProperties::reliableWrite | services::GattCharacteristicExtendedProperties::writableAuxiliaries;

    EXPECT_EQ(services::GattCharacteristicExtendedProperties::reliableWrite,
        both & services::GattCharacteristicExtendedProperties::reliableWrite);
    EXPECT_EQ(services::GattCharacteristicExtendedProperties::writableAuxiliaries,
        both & services::GattCharacteristicExtendedProperties::writableAuxiliaries);
    EXPECT_EQ(services::GattCharacteristicExtendedProperties::none,
        services::GattCharacteristicExtendedProperties::reliableWrite & services::GattCharacteristicExtendedProperties::writableAuxiliaries);
}

TEST(GattServiceChangedTest, decodes_a_service_changed_value)
{
    // Little-endian on the wire: start 0x0001, end 0xFFFF.
    const std::array<uint8_t, 4> value{ 0x01, 0x00, 0xFF, 0xFF };

    auto serviceChanged = services::GattServiceChangedFromValue(infra::MakeConstByteRange(value));

    ASSERT_TRUE(serviceChanged);
    EXPECT_EQ(0x0001, serviceChanged->startHandle);
    EXPECT_EQ(0xFFFF, serviceChanged->endHandle);
}

TEST(GattServiceChangedTest, decodes_the_byte_order_of_the_wire_not_of_the_host)
{
    const std::array<uint8_t, 4> value{ 0x34, 0x12, 0x78, 0x56 };

    auto serviceChanged = services::GattServiceChangedFromValue(infra::MakeConstByteRange(value));

    ASSERT_TRUE(serviceChanged);
    EXPECT_EQ(0x1234, serviceChanged->startHandle);
    EXPECT_EQ(0x5678, serviceChanged->endHandle);
}

TEST(GattServiceChangedTest, decodes_a_value_at_an_odd_offset)
{
    // The payload is viewed starting one byte in, so the handles are unaligned.
    const std::array<uint8_t, 5> storage{ 0xAA, 0x34, 0x12, 0x78, 0x56 };

    auto serviceChanged = services::GattServiceChangedFromValue(infra::MakeRange(storage.begin() + 1, storage.end()));

    ASSERT_TRUE(serviceChanged);
    EXPECT_EQ(0x1234, serviceChanged->startHandle);
    EXPECT_EQ(0x5678, serviceChanged->endHandle);
}

TEST(GattServiceChangedTest, rejects_a_value_that_is_too_short)
{
    const std::array<uint8_t, 3> value{ 0x01, 0x00, 0xFF };

    EXPECT_FALSE(services::GattServiceChangedFromValue(infra::MakeConstByteRange(value)));
}

TEST(GattServiceChangedTest, rejects_a_value_that_is_too_long)
{
    const std::array<uint8_t, 5> value{ 0x01, 0x00, 0xFF, 0xFF, 0x00 };

    EXPECT_FALSE(services::GattServiceChangedFromValue(infra::MakeConstByteRange(value)));
}

TEST(GattServiceChangedTest, rejects_an_empty_value)
{
    EXPECT_FALSE(services::GattServiceChangedFromValue(infra::ConstByteRange()));
}

TEST(GattIncludedServiceTest, holds_the_handle_range_of_the_included_service)
{
    services::GattIncludedService includedService{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x5, 0x20, 0x2F };

    EXPECT_EQ(0x5, includedService.Handle());
    EXPECT_EQ(0x20, includedService.ServiceHandle());
    EXPECT_EQ(0x2F, includedService.ServiceEndHandle());
    EXPECT_EQ(services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), includedService.Type());
}

TEST(GattIncludedServiceTest, compares_by_value)
{
    services::GattIncludedService first{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x5, 0x20, 0x2F };
    services::GattIncludedService same{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x5, 0x20, 0x2F };
    services::GattIncludedService different{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x5, 0x20, 0x30 };

    EXPECT_EQ(first, same);
    EXPECT_NE(first, different);
}
