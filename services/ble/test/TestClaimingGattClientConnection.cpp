#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/Att.hpp"
#include "services/ble/ClaimingGattClientConnection.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace
{
    class ClaimingGattClientConnectionTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<services::GattClientConnectionMock> connection;
        services::ClaimingGattClientConnection adapter{ connection };
        testing::StrictMock<services::GattClientConnectionObserverMock> connectionObserver{ adapter };
        testing::StrictMock<services::GattClientUpdateObserverMock> updateObserver{ adapter };

        const services::AttAttribute::Handle handle = 0x1;
        const services::AttAttribute::Handle endHandle = 0x2;
        const std::array<uint8_t, 4> dataStorage{ 0x01, 0x02, 0x03, 0x04 };

        infra::Function<void(services::GattResult)> ignoredResult;
        infra::Function<void(services::GattResult, infra::ConstByteRange)> ignoredReadResult;
    };
}

TEST_F(ClaimingGattClientConnectionTest, should_call_discover_services)
{
    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_discover_characteristics)
{
    EXPECT_CALL(connection, DiscoverCharacteristics(handle, endHandle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverCharacteristics(handle, endHandle, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_discover_descriptors)
{
    EXPECT_CALL(connection, DiscoverDescriptors(handle, endHandle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverDescriptors(handle, endHandle, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, discovers_a_whole_service_through_the_decorator)
{
    const services::GattService service{ services::AttAttribute::Uuid16{ 0x180D }, 0x10, 0x1f };

    EXPECT_CALL(connection, DiscoverCharacteristics(0x10, 0x1f, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverCharacteristics(service, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, discovers_the_descriptors_of_a_whole_service_through_the_decorator)
{
    const services::GattService service{ services::AttAttribute::Uuid16{ 0x180D }, 0x10, 0x1f };

    EXPECT_CALL(connection, DiscoverDescriptors(0x10, 0x1f, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverDescriptors(service, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_forward_service_discovered)
{
    static const services::GattService service{ services::AttAttribute::Uuid16{ 0x180D }, 0x1, 0x2 };

    EXPECT_CALL(connectionObserver, ServiceDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.ServiceDiscovered(service);
        });
}

TEST_F(ClaimingGattClientConnectionTest, should_forward_characteristic_discovered)
{
    static const services::GattCharacteristic characteristic{ services::AttAttribute::Uuid16{ 0x180D }, 0x1, 0x2, services::GattCharacteristic::PropertyFlags::read | services::GattCharacteristic::PropertyFlags::notify };

    EXPECT_CALL(connectionObserver, CharacteristicDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.CharacteristicDiscovered(characteristic);
        });
}

TEST_F(ClaimingGattClientConnectionTest, should_forward_descriptor_discovered)
{
    static const services::GattDescriptor descriptor{ services::AttAttribute::Uuid16{ 0x2902 }, 0x1 };

    EXPECT_CALL(connectionObserver, DescriptorDiscovered(testing::_));
    connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([](auto& observer)
        {
            observer.DescriptorDiscovered(descriptor);
        });
}

TEST_F(ClaimingGattClientConnectionTest, should_release_discovery_claim_on_completion)
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

TEST_F(ClaimingGattClientConnectionTest, should_refuse_a_second_discovery_while_one_is_in_flight)
{
    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    EXPECT_EQ(services::GattRequestStatus::busy, adapter.DiscoverServices(ignoredResult));

    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_report_a_refused_discovery_through_on_done)
{
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::disconnected };

    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_read_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_call_write_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_pass_write_without_response_straight_through)
{
    const infra::ConstByteRange data = infra::MakeRange(dataStorage);

    EXPECT_CALL(connection, WriteWithoutResponse(handle, infra::ByteRangeContentsEqual(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteWithoutResponse(handle, data));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_enable_notification_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_call_disable_notification_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_call_enable_indication_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_call_disable_indication_characteristic)
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

TEST_F(ClaimingGattClientConnectionTest, should_refuse_a_second_characteristic_operation_while_one_is_in_flight)
{
    infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadDone;
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::success, infra::ConstByteRange() };

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<1>(&onReadDone), testing::Return(services::GattRequestStatus::accepted)));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, onDone));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::busy, adapter.Read(handle, ignoredReadResult));

    onReadDone(services::GattResult::success, infra::ConstByteRange());
}

TEST_F(ClaimingGattClientConnectionTest, should_report_a_refused_read_through_on_done)
{
    infra::VerifyingFunction<void(services::GattResult, infra::ConstByteRange)> onDone{ services::GattResult::unsupported, infra::ConstByteRange() };

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::notSupported));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, onDone));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_mtu_exchange)
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

TEST_F(ClaimingGattClientConnectionTest, should_block_characteristic_operation_while_discovering)
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

TEST_F(ClaimingGattClientConnectionTest, should_forward_notification_received)
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

TEST_F(ClaimingGattClientConnectionTest, should_forward_indication_received)
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

TEST_F(ClaimingGattClientConnectionTest, should_release_the_claim_when_an_operation_fails_after_the_link_is_gone)
{
    infra::Function<void(services::GattResult)> onExchangeMtuDone;
    infra::VerifyingFunction<void(services::GattResult)> onDone{ services::GattResult::disconnected };

    EXPECT_CALL(connection, ExchangeMtu(testing::_)).WillOnce(testing::DoAll(testing::SaveArg<0>(&onExchangeMtuDone), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ExchangeMtu(onDone));
    ExecuteAllActions();

    onExchangeMtuDone(services::GattResult::disconnected);

    EXPECT_CALL(connection, ExchangeMtu(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ExchangeMtu(ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_drain_a_queued_operation_when_the_link_is_gone)
{
    infra::Function<void(services::GattResult)> onDiscoveryDone;
    infra::VerifyingFunction<void(services::GattResult)> onDiscoveryComplete{ services::GattResult::disconnected };
    infra::VerifyingFunction<void(services::GattResult)> onDisableDone{ services::GattResult::disconnected };

    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::DoAll(testing::SaveArg<0>(&onDiscoveryDone), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(onDiscoveryComplete));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DisableIndication(handle, onDisableDone));
    ExecuteAllActions();

    EXPECT_CALL(connection, DisableIndication(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::invalidState));
    onDiscoveryDone(services::GattResult::disconnected);
    ExecuteAllActions();
}

namespace
{
    // A stack whose MTU is 23 gives a 20-byte read chunk and a 18-byte prepare chunk.
    constexpr uint16_t testMtu = 23;
    constexpr std::size_t readChunk = testMtu - 1;
    constexpr std::size_t writeChunk = testMtu - 5;

    class LongOperationsTest
        : public ClaimingGattClientConnectionTest
    {
    public:
        LongOperationsTest()
        {
            EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillRepeatedly(testing::Return(testMtu));
        }

        static std::array<uint8_t, readChunk> FullChunk(uint8_t first)
        {
            std::array<uint8_t, readChunk> chunk{};
            for (std::size_t i = 0; i != chunk.size(); ++i)
                chunk[i] = static_cast<uint8_t>(first + i);
            return chunk;
        }

        infra::BoundedVector<uint8_t>::WithMaxSize<64> value;
    };
}

TEST_F(LongOperationsTest, should_read_a_short_value_without_a_blob_request)
{
    const std::array<uint8_t, 3> stored{ 0xA, 0xB, 0xC };

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success, infra::MakeRange(stored)), testing::Return(services::GattRequestStatus::accepted)));

    // The reported range views the caller's buffer, not the stack's, so contents are what
    // matter here rather than range identity.
    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::success, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();

    EXPECT_THAT(value, testing::ElementsAreArray(stored));
}

TEST_F(LongOperationsTest, should_read_a_blob_when_the_first_response_fills_the_mtu)
{
    auto first = FullChunk(0);
    const std::array<uint8_t, 2> second{ 0xEE, 0xFF };

    testing::InSequence sequence;
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success, infra::MakeRange(first)), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ReadBlob(handle, readChunk, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::GattResult::success, infra::MakeRange(second)), testing::Return(services::GattRequestStatus::accepted)));

    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::success, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();

    EXPECT_EQ(readChunk + 2, value.size());
    EXPECT_EQ(0xEE, value[readChunk]);
}

TEST_F(LongOperationsTest, should_treat_an_invalid_offset_after_a_full_chunk_as_the_end_of_the_value)
{
    auto first = FullChunk(0);

    testing::InSequence sequence;
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success, infra::MakeRange(first)), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ReadBlob(handle, readChunk, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::GattResult::invalidLength, infra::ConstByteRange()), testing::Return(services::GattRequestStatus::accepted)));

    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::success, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();

    EXPECT_EQ(readChunk, value.size());
}

TEST_F(LongOperationsTest, should_report_a_read_failure_that_is_not_an_end_of_value)
{
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::insufficientAuthentication, infra::ConstByteRange()), testing::Return(services::GattRequestStatus::accepted)));

    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::insufficientAuthentication, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();

    EXPECT_TRUE(value.empty());
}

TEST_F(LongOperationsTest, should_report_insufficient_resources_when_the_value_does_not_fit)
{
    infra::BoundedVector<uint8_t>::WithMaxSize<readChunk + 1> small;
    auto first = FullChunk(0);
    const std::array<uint8_t, 4> second{ 1, 2, 3, 4 };

    testing::InSequence sequence;
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success, infra::MakeRange(first)), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ReadBlob(handle, readChunk, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::GattResult::success, infra::MakeRange(second)), testing::Return(services::GattRequestStatus::accepted)));

    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::insufficientResources, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, small, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();

    EXPECT_TRUE(small.full());
}

TEST_F(LongOperationsTest, should_refuse_a_second_operation_while_a_long_read_is_outstanding)
{
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, ignoredReadResult));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::busy, adapter.Read(handle, ignoredReadResult));
    EXPECT_EQ(services::GattRequestStatus::busy, adapter.WriteLong(handle, infra::MakeRange(dataStorage), ignoredResult));
}

TEST_F(LongOperationsTest, should_write_a_short_value_without_preparing)
{
    EXPECT_CALL(connection, Write(handle, testing::ElementsAreArray(dataStorage), testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::GattResult::success), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> onDone(services::GattResult::success);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(dataStorage), onDone));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_prepare_one_write_per_chunk_then_execute)
{
    std::array<uint8_t, writeChunk + 3> payload{};
    for (std::size_t i = 0; i != payload.size(); ++i)
        payload[i] = static_cast<uint8_t>(i);

    auto firstChunk = infra::ConstByteRange(payload.begin(), payload.begin() + writeChunk);
    auto secondChunk = infra::ConstByteRange(payload.begin() + writeChunk, payload.end());

    testing::InSequence sequence;
    EXPECT_CALL(connection, PrepareWrite(handle, 0, testing::ElementsAreArray(firstChunk), testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::success, 0, firstChunk), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, PrepareWrite(handle, writeChunk, testing::ElementsAreArray(secondChunk), testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::success, writeChunk, secondChunk), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::write, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> onDone(services::GattResult::success);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(payload), onDone));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_cancel_the_prepared_writes_when_a_prepare_fails)
{
    std::array<uint8_t, writeChunk + 3> payload{};
    auto firstChunk = infra::ConstByteRange(payload.begin(), payload.begin() + writeChunk);

    testing::InSequence sequence;
    EXPECT_CALL(connection, PrepareWrite(handle, 0, testing::ElementsAreArray(firstChunk), testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::success, 0, firstChunk), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, PrepareWrite(handle, writeChunk, testing::_, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::insufficientResources, 0, infra::ConstByteRange()), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::cancel, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> onDone(services::GattResult::insufficientResources);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(payload), onDone));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_cancel_the_prepared_writes_when_the_echoed_value_does_not_match)
{
    std::array<uint8_t, writeChunk + 3> payload{};
    const std::array<uint8_t, writeChunk> wrong{ 0xFF };

    testing::InSequence sequence;
    EXPECT_CALL(connection, PrepareWrite(handle, 0, testing::_, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::success, 0, infra::MakeRange(wrong)), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::cancel, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::success), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> onDone(services::GattResult::unknown);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(payload), onDone));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_report_the_original_failure_and_not_the_cancel_result)
{
    std::array<uint8_t, writeChunk + 3> payload{};

    testing::InSequence sequence;
    EXPECT_CALL(connection, PrepareWrite(handle, 0, testing::_, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<3>(services::GattResult::notPermitted, 0, infra::ConstByteRange()), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::cancel, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::GattResult::timeout), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> onDone(services::GattResult::notPermitted);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(payload), onDone));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_release_the_claim_once_a_long_write_completes)
{
    EXPECT_CALL(connection, Write(handle, testing::_, testing::_)).WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::GattResult::success), testing::Return(services::GattRequestStatus::accepted)));

    infra::VerifyingFunction<void(services::GattResult)> writeDone(services::GattResult::success);
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.WriteLong(handle, infra::MakeRange(dataStorage), writeDone));
    ExecuteAllActions();

    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.Read(handle, ignoredReadResult));
    ExecuteAllActions();
}

TEST_F(LongOperationsTest, should_report_disconnected_when_the_underlying_read_is_refused)
{
    // ReadLong already returned accepted, having taken the claim, so the refusal further down
    // has to be reported through onDone rather than swallowed.
    EXPECT_CALL(connection, Read(handle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    infra::MockCallback<void(services::GattResult, infra::ConstByteRange)> onDone;
    EXPECT_CALL(onDone, callback(services::GattResult::disconnected, testing::_));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.ReadLong(handle, value, [&onDone](services::GattResult result, infra::ConstByteRange data)
                                                         {
                                                             onDone.callback(result, data);
                                                         }));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_call_discover_included_services)
{
    EXPECT_CALL(connection, DiscoverIncludedServices(handle, endHandle, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverIncludedServices(handle, endHandle, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_discover_the_included_services_of_a_whole_service)
{
    const services::GattService service{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x10, 0x1F };

    EXPECT_CALL(connection, DiscoverIncludedServices(0x10, 0x1F, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverIncludedServices(service, ignoredResult));
    ExecuteAllActions();
}

TEST_F(ClaimingGattClientConnectionTest, should_forward_an_included_service_discovered)
{
    const services::GattIncludedService includedService{ services::AttAttribute::Uuid(services::AttAttribute::Uuid16{ 0x180A }), 0x5, 0x20, 0x2F };

    EXPECT_CALL(connectionObserver, IncludedServiceDiscovered(includedService));
    adapter.IncludedServiceDiscovered(includedService);
}

TEST_F(ClaimingGattClientConnectionTest, should_queue_an_included_service_discovery_behind_another_discovery)
{
    EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, adapter.DiscoverServices(ignoredResult));
    ExecuteAllActions();

    EXPECT_EQ(services::GattRequestStatus::busy, adapter.DiscoverIncludedServices(handle, endHandle, ignoredResult));
}
