#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/Att.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"
#include <cstdint>

namespace
{
    services::AttAttribute::Uuid16 uuid16{ 0x42 };
    services::AttAttribute::Uuid128 uuid128{ { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 } };

    using GattPropertyFlags = services::GattCharacteristic::PropertyFlags;

    class GattClientServiceTest
        : public testing::Test
    {
    public:
        testing::StrictMock<services::GattClientConnectionMock> connection;
        services::GattClientService service{ uuid16, 0x1, 0x9 };
    };
}

TEST_F(GattClientServiceTest, characteristic_supports_different_uuid_lengths)
{
    services::GattClientCharacteristic characteristicDefinitionA{ connection, uuid16, 0x2, 0x3, GattPropertyFlags::none };
    services::GattClientCharacteristic characteristicDefinitionB{ connection, uuid128, 0x2, 0x3, GattPropertyFlags::none };

    service.AddCharacteristic(characteristicDefinitionA);
    service.AddCharacteristic(characteristicDefinitionB);

    EXPECT_EQ(0x42, std::get<services::AttAttribute::Uuid16>(characteristicDefinitionA.Type()));
    EXPECT_EQ(uuid128, std::get<services::AttAttribute::Uuid128>(characteristicDefinitionB.Type()));
}

TEST_F(GattClientServiceTest, characteristic_supports_different_properties)
{
    services::GattClientCharacteristic characteristicDefinitionA{ connection, uuid16, 0x2, 0x3, GattPropertyFlags::write | GattPropertyFlags::indicate };
    services::GattClientCharacteristic characteristicDefinitionB{ connection, uuid16, 0x2, 0x3, GattPropertyFlags::broadcast };

    service.AddCharacteristic(characteristicDefinitionA);
    service.AddCharacteristic(characteristicDefinitionB);

    EXPECT_EQ(GattPropertyFlags::write | GattPropertyFlags::indicate, characteristicDefinitionA.Properties());
    EXPECT_EQ(GattPropertyFlags::broadcast, characteristicDefinitionB.Properties());
}

TEST_F(GattClientServiceTest, characteristic_is_added_to_service)
{
    services::GattClientCharacteristic characteristicDefinitionA{ connection, uuid16, 0x2, 0x3, GattPropertyFlags::write };
    services::GattClientCharacteristic characteristicDefinitionB{ connection, services::AttAttribute::Uuid16(0x84), 0x4, 0x5, GattPropertyFlags::none };

    service.AddCharacteristic(characteristicDefinitionA);
    service.AddCharacteristic(characteristicDefinitionB);

    EXPECT_FALSE(service.Characteristics().empty());
    EXPECT_EQ(0x84, std::get<services::AttAttribute::Uuid16>(service.Characteristics().front().Type()));
}

class GattClientCharacteristicTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    GattClientCharacteristicTest()
        : service(uuid16, 0x1, 0x9)
        , characteristic(connection, uuid16, characteristicHandle, characteristicValueHandle, GattPropertyFlags::write)
    {
        gattUpdateObserver.Attach(characteristic);
    }

    static const services::AttAttribute::Handle characteristicHandle = 0x2;
    static const services::AttAttribute::Handle characteristicValueHandle = 0x3;

    testing::StrictMock<services::GattClientConnectionMock> connection;
    services::GattClientService service;
    services::GattClientCharacteristic characteristic;
    testing::StrictMock<services::GattClientCharacteristicUpdateObserverMock> gattUpdateObserver;
};

TEST_F(GattClientCharacteristicTest, receives_valid_notification_should_notify_observers)
{
    const auto data = infra::MakeStringByteRange("string");

    EXPECT_CALL(gattUpdateObserver, NotificationReceived(infra::ByteRangeContentsEqual(data)));
    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([&data](auto& observer)
        {
            observer.NotificationReceived(characteristicValueHandle, data);
        });
}

TEST_F(GattClientCharacteristicTest, receives_valid_indication_should_notify_observers)
{
    EXPECT_CALL(gattUpdateObserver, IndicationReceived(infra::ByteRangeContentsEqual(infra::MakeStringByteRange("string")), testing::_))
        .WillOnce(testing::InvokeArgument<1>());

    connection.NotifyIndicationReceived(characteristicValueHandle, infra::MakeStringByteRange("string"), infra::MockFunction<void()>());
}

TEST_F(GattClientCharacteristicTest, receives_invalid_notification_should_not_notify_observers)
{
    const services::AttAttribute::Handle invalidCharacteristicValueHandle = 0x7;

    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([&invalidCharacteristicValueHandle](auto& observer)
        {
            observer.NotificationReceived(invalidCharacteristicValueHandle, infra::MakeStringByteRange("string"));
        });
}

TEST_F(GattClientCharacteristicTest, receives_invalid_indication_should_not_notify_observers)
{
    const services::AttAttribute::Handle invalidCharacteristicValueHandle = 0x7;

    connection.NotifyIndicationReceived(invalidCharacteristicValueHandle, infra::MakeStringByteRange("string"), infra::MockFunction<void()>());
}

class GattClientCharacteristicWithDistantValueHandleTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    GattClientCharacteristicWithDistantValueHandleTest()
    {
        gattUpdateObserver.Attach(characteristic);
    }

    static const services::AttAttribute::Handle characteristicHandle = 0x2;
    static const services::AttAttribute::Handle characteristicValueHandle = 0x9;

    testing::StrictMock<services::GattClientConnectionMock> connection;
    services::GattClientCharacteristic characteristic{ connection, uuid16, characteristicHandle, characteristicValueHandle, GattPropertyFlags::notify };
    testing::StrictMock<services::GattClientCharacteristicUpdateObserverMock> gattUpdateObserver;
};

TEST_F(GattClientCharacteristicWithDistantValueHandleTest, notification_on_the_value_handle_notifies_observers)
{
    const auto data = infra::MakeStringByteRange("string");

    EXPECT_CALL(gattUpdateObserver, NotificationReceived(infra::ByteRangeContentsEqual(data)));
    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([&data](auto& observer)
        {
            observer.NotificationReceived(characteristicValueHandle, data);
        });
}

TEST_F(GattClientCharacteristicWithDistantValueHandleTest, notification_just_past_the_declaration_handle_is_ignored)
{
    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([](auto& observer)
        {
            observer.NotificationReceived(characteristicHandle + 1, infra::MakeStringByteRange("string"));
        });
}

TEST_F(GattClientCharacteristicWithDistantValueHandleTest, indication_on_the_value_handle_notifies_observers)
{
    EXPECT_CALL(gattUpdateObserver, IndicationReceived(infra::ByteRangeContentsEqual(infra::MakeStringByteRange("string")), testing::_))
        .WillOnce(testing::InvokeArgument<1>());

    connection.NotifyIndicationReceived(characteristicValueHandle, infra::MakeStringByteRange("string"), infra::MockFunction<void()>());
}

TEST_F(GattClientCharacteristicWithDistantValueHandleTest, indication_just_past_the_declaration_handle_is_ignored)
{
    connection.NotifyIndicationReceived(characteristicHandle + 1, infra::MakeStringByteRange("string"), infra::MockFunction<void()>());
}

TEST_F(GattClientCharacteristicTest, should_read_characteristic_and_callback_with_data_received)
{
    const auto data = infra::MakeStringByteRange("string");
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::success, data };

    EXPECT_CALL(connection, Read(characteristicValueHandle, ::testing::_))
        .WillOnce([&data](services::AttAttribute::Handle, infra::Function<void(services::GattResult, infra::ConstByteRange)> onDone)
            {
                onDone(services::GattResult::success, data);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.Read(onDone));
}

TEST_F(GattClientCharacteristicTest, should_write_characteristic_and_callback)
{
    const auto data = infra::MakeStringByteRange("string");
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, Write(characteristicValueHandle, infra::ByteRangeContentsEqual(data), testing::_)).WillOnce([](services::AttAttribute::Handle, infra::ConstByteRange, infra::Function<void(services::GattResult)> onDone)
        {
            onDone(services::GattResult::success);
            return services::GattRequestStatus::accepted;
        });
    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.Write(data, onDone));
}

TEST_F(GattClientCharacteristicTest, should_write_without_response_characteristic)
{
    const auto data = infra::MakeStringByteRange("string");

    EXPECT_CALL(connection, WriteWithoutResponse(characteristicValueHandle, infra::ByteRangeContentsEqual(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.WriteWithoutResponse(data));
}

TEST_F(GattClientCharacteristicTest, should_enable_notification_characteristic_and_callback)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, EnableNotification(characteristicValueHandle, ::testing::_)).WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDone)
        {
            onDone(services::GattResult::success);
            return services::GattRequestStatus::accepted;
        });

    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.EnableNotification(onDone));
}

TEST_F(GattClientCharacteristicTest, should_disable_notification_characteristic_and_callback)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, DisableNotification(characteristicValueHandle, ::testing::_)).WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDone)
        {
            onDone(services::GattResult::success);
            return services::GattRequestStatus::accepted;
        });

    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.DisableNotification(onDone));
}

TEST_F(GattClientCharacteristicTest, should_enable_indication_characteristic_and_callback)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, EnableIndication(characteristicValueHandle, ::testing::_)).WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDone)
        {
            onDone(services::GattResult::success);
            return services::GattRequestStatus::accepted;
        });
    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.EnableIndication(onDone));
}

TEST_F(GattClientCharacteristicTest, should_disable_indication_characteristic_and_callback)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, DisableIndication(characteristicValueHandle, ::testing::_)).WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDone)
        {
            onDone(services::GattResult::success);
            return services::GattRequestStatus::accepted;
        });
    EXPECT_EQ(services::GattRequestStatus::accepted, characteristic.DisableIndication(onDone));
}

class GattClientTwoCharacteristicsTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    static const services::AttAttribute::Handle firstValueHandle = 0x3;
    static const services::AttAttribute::Handle secondValueHandle = 0x6;

    testing::StrictMock<services::GattClientConnectionMock> connection;
    services::GattClientCharacteristic first{ connection, uuid16, 0x2, firstValueHandle, GattPropertyFlags::indicate };
    services::GattClientCharacteristic second{ connection, uuid16, 0x5, secondValueHandle, GattPropertyFlags::indicate };
};

TEST_F(GattClientTwoCharacteristicsTest, an_indication_is_acknowledged_exactly_once)
{
    infra::VerifyingFunction<void()> onDone;

    connection.NotifyIndicationReceived(firstValueHandle, infra::MakeStringByteRange("string"), onDone);
}

TEST_F(GattClientTwoCharacteristicsTest, an_indication_for_no_characteristic_is_still_acknowledged_once)
{
    infra::VerifyingFunction<void()> onDone;

    connection.NotifyIndicationReceived(0x9, infra::MakeStringByteRange("string"), onDone);
}

TEST_F(GattClientTwoCharacteristicsTest, an_indication_is_acknowledged_only_after_the_matching_observer_finishes)
{
    testing::StrictMock<services::GattClientCharacteristicUpdateObserverMock> firstObserver{ first };
    infra::Function<void()> observerDone;
    infra::VerifyingFunction<void()> onDone;

    EXPECT_CALL(firstObserver, IndicationReceived(testing::_, testing::_)).WillOnce(testing::SaveArg<1>(&observerDone));

    connection.NotifyIndicationReceived(firstValueHandle, infra::MakeStringByteRange("string"), onDone);

    observerDone();
}
