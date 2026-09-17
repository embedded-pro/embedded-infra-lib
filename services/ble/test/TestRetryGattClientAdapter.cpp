#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/RetryGattClientCharacteristicsOperations.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"

namespace
{
    class RetryGattClientCharacteristicsOperationsTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<services::GattClientConnectionMock> connection;
        services::RetryGattClientCharacteristicsOperations retryAdapter{ connection };
        testing::StrictMock<services::GattClientUpdateObserverMock> updateObserver{ retryAdapter };

        const uint16_t handle = 0x1;
        std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
        infra::Function<void(services::GattResult)> onDoneMock;
    };
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_read_characteristic)
{
    infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadMock;

    EXPECT_CALL(connection, Read(handle, testing::Ref(onReadMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.Read(handle, onReadMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_write_characteristic)
{
    EXPECT_CALL(connection, Write(handle, testing::ElementsAreArray(data), testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.Write(handle, data, onDoneMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_not_retry_write_without_response_when_accepted)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.WriteWithoutResponse(handle, data));
    ExecuteAllActions();
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_retry_write_without_response_while_busy)
{
    testing::InSequence sequence;

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.WriteWithoutResponse(handle, data));
    ExecuteAllActions();
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_stop_retrying_write_without_response_when_refused)
{
    testing::InSequence sequence;

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.WriteWithoutResponse(handle, data));
    ExecuteAllActions();
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_refuse_a_second_write_without_response_while_retrying)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.WriteWithoutResponse(handle, data));
    EXPECT_EQ(services::GattRequestStatus::busy, retryAdapter.WriteWithoutResponse(handle, data));

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    ExecuteAllActions();
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_enable_notification_characteristic)
{
    EXPECT_CALL(connection, EnableNotification(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.EnableNotification(handle, onDoneMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_disable_notification_characteristic)
{
    EXPECT_CALL(connection, DisableNotification(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.DisableNotification(handle, onDoneMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_enable_indication_characteristic)
{
    EXPECT_CALL(connection, EnableIndication(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.EnableIndication(handle, onDoneMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_call_disable_indication_characteristic)
{
    EXPECT_CALL(connection, DisableIndication(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryAdapter.DisableIndication(handle, onDoneMock));
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_forward_notification_received)
{
    EXPECT_CALL(updateObserver, NotificationReceived(handle, testing::ElementsAreArray(data)));
    retryAdapter.NotificationReceived(handle, data);
}

TEST_F(RetryGattClientCharacteristicsOperationsTest, should_forward_indication_received)
{
    infra::VerifyingFunction<void()> onDone;

    EXPECT_CALL(updateObserver, IndicationReceived(handle, testing::ElementsAreArray(data), testing::_)).WillOnce(testing::InvokeArgument<2>());
    retryAdapter.IndicationReceived(handle, data, onDone);
}
