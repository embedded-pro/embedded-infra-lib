#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/Att.hpp"
#include "services/ble/ClaimingGattClientAdapter.hpp"
#include "services/ble/test_doubles/GapCentralMock.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace
{
    class ClaimingGattClientAdapterTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<services::GattClientConnectionMock> connection;
        testing::StrictMock<services::GapCentralMock> gapCentral;
        services::ClaimingGattClientAdapter adapter{ connection, gapCentral };
        testing::StrictMock<services::GattClientConnectionObserverMock> connectionObserver{ adapter };
        testing::StrictMock<services::GattClientUpdateObserverMock> updateObserver{ adapter };

        const services::AttAttribute::Handle handle = 0x1;
        const services::AttAttribute::Handle endHandle = 0x2;
        const std::array<uint8_t, 4> dataStorage{ 0x01, 0x02, 0x03, 0x04 };

        infra::Function<void(services::GattResult)> ignoredResult;
        infra::Function<void(services::GattResult, infra::ConstByteRange)> ignoredReadResult;
    };
}

TEST_F(ClaimingGattClientAdapterTest, should_call_discover_services)
{
    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_discover_characteristics)
{
    EXPECT_CALL(connection, DiscoverCharacteristics(handle, endHandle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverCharacteristics(handle, endHandle, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_discover_descriptors)
{
    EXPECT_CALL(connection, DiscoverDescriptors(handle, endHandle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverDescriptors(handle, endHandle, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_forward_service_discovered)
{
    static const services::GattService service{ services::AttAttribute::Uuid16{ 0x180D }, 0x1, 0x2 };

    EXPECT_CALL(connectionObserver, ServiceDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.ServiceDiscovered(service);
        });
}

TEST_F(ClaimingGattClientAdapterTest, should_forward_characteristic_discovered)
{
    static const services::GattCharacteristic characteristic{ services::AttAttribute::Uuid16{ 0x180D }, 0x1, 0x2, services::GattCharacteristic::PropertyFlags::read | services::GattCharacteristic::PropertyFlags::notify };

    EXPECT_CALL(connectionObserver, CharacteristicDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.CharacteristicDiscovered(characteristic);
        });
}

TEST_F(ClaimingGattClientAdapterTest, should_forward_descriptor_discovered)
{
    static const services::GattDescriptor descriptor{ services::AttAttribute::Uuid16{ 0x2902 }, 0x1 };

    EXPECT_CALL(connectionObserver, DescriptorDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.DescriptorDiscovered(descriptor);
        });
}

TEST_F(ClaimingGattClientAdapterTest, should_release_discovery_claim_on_completion)
{
    infra::Function<void(services::GattResult)> onDiscoverServicesDone;
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::DoAll(testing::SaveArg<0>(&onDiscoverServicesDone), testing::Return(services::GattRequestStatus::accepted)));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(onDone));
    ExecuteAllActions();

    onDiscoverServicesDone(services::GattResult::success);

    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_refuse_a_second_discovery_while_one_is_in_flight)
{
    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    EXPECT_EQ(services::GattRequestStatus::busy, adapter.DiscoverServices(ignoredResult));

    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_report_a_refused_discovery_through_on_done)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::disconnected };

    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_read_characteristic)
{
    const infra::ConstByteRange readResult = infra::MakeRange(dataStorage);
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::success, readResult };

    EXPECT_CALL(connection, Read(handle, testing::_))
        .WillOnce([&readResult](services::AttAttribute::Handle, infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadDone)
            {
                onReadDone(services::GattResult::success, readResult);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_write_characteristic)
{
    const infra::ConstByteRange data = infra::MakeRange(dataStorage);
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, Write(handle, infra::ByteRangeContentsEqual(data), testing::_))
        .WillOnce([](services::AttAttribute::Handle, infra::ConstByteRange, const infra::Function<void(services::GattResult)>& onWriteDone)
            {
                onWriteDone(services::GattResult::success);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Write(handle, data, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_pass_write_without_response_straight_through)
{
    const infra::ConstByteRange data = infra::MakeRange(dataStorage);

    EXPECT_CALL(connection, WriteWithoutResponse(handle, infra::ByteRangeContentsEqual(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteWithoutResponse(handle, data));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_enable_notification_characteristic)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, EnableNotification(handle, testing::_))
        .WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onEnableDone)
            {
                onEnableDone(services::GattResult::success);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.EnableNotification(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_disable_notification_characteristic)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, DisableNotification(handle, testing::_))
        .WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDisableDone)
            {
                onDisableDone(services::GattResult::success);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DisableNotification(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_enable_indication_characteristic)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, EnableIndication(handle, testing::_))
        .WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onEnableDone)
            {
                onEnableDone(services::GattResult::success);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.EnableIndication(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_disable_indication_characteristic)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::success };

    EXPECT_CALL(connection, DisableIndication(handle, testing::_))
        .WillOnce([](services::AttAttribute::Handle, infra::Function<void(services::GattResult)> onDisableDone)
            {
                onDisableDone(services::GattResult::success);
                return services::GattRequestStatus::accepted;
            });

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DisableIndication(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_refuse_a_second_characteristic_operation_while_one_is_in_flight)
{
    infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadDone;
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::success, infra::ConstByteRange() };

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<1>(&onReadDone), testing::Return(services::GattRequestStatus::accepted)));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, onDone));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::busy, adapter.Read(handle, ignoredReadResult));

    onReadDone(services::GattResult::success, infra::ConstByteRange());
}

TEST_F(ClaimingGattClientAdapterTest, should_report_a_refused_read_through_on_done)
{
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::unsupported, infra::ConstByteRange() };

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::notSupported));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_call_mtu_exchange)
{
    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(200));
    EXPECT_EQ(200, adapter.EffectiveMaxAttMtuSize());

    EXPECT_CALL(connection, ExchangeMtu(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ExchangeMtu(ignoredResult));
    ExecuteAllActions();

    EXPECT_CALL(connectionObserver, MtuChanged(247));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.MtuChanged(247);
        });
}

TEST_F(ClaimingGattClientAdapterTest, should_block_characteristic_operation_while_discovering)
{
    infra::Function<void(services::GattResult)> onDiscoveryDone;
    infra::VerifyingFunction<void(services::GattResult)> onDiscoveryComplete{ services::GattResult::success };

    EXPECT_CALL(connection, DiscoverCharacteristics(handle, endHandle, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<2>(&onDiscoveryDone), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverCharacteristics(handle, endHandle, onDiscoveryComplete));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DisableIndication(handle, ignoredResult));
    ExecuteAllActions();

    EXPECT_CALL(connection, DisableIndication(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    onDiscoveryDone(services::GattResult::success);
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_forward_notification_received)
{
    static const auto notifiedHandle = 0x1;
    static const infra::ConstByteRange data = infra::MakeRange(std::array<uint8_t, 4>{ 0x01, 0x02, 0x03, 0x04 });

    EXPECT_CALL(updateObserver, NotificationReceived(notifiedHandle, data));
    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([](auto& observer)
        {
            observer.NotificationReceived(notifiedHandle, data);
        });
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_forward_indication_received)
{
    static const auto notifiedHandle = 0x1;
    static const infra::ConstByteRange data = infra::MakeRange(std::array<uint8_t, 4>{ 0x01, 0x02, 0x03, 0x04 });

    EXPECT_CALL(updateObserver, IndicationReceived(notifiedHandle, data, testing::_)).WillOnce(testing::InvokeArgument<2>());
    connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([](auto& observer)
        {
            observer.IndicationReceived(notifiedHandle, data, infra::MockFunction<void()>());
        });
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientAdapterTest, should_release_claimer_when_disconnected)
{
    EXPECT_CALL(connection, ExchangeMtu(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ExchangeMtu(ignoredResult));
    ExecuteAllActions();

    gapCentral.ChangeState(services::GapCentralState::standby);

    EXPECT_CALL(connection, ExchangeMtu(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ExchangeMtu(ignoredResult));
    ExecuteAllActions();
}
