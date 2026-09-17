#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/RetryingGattClientConnection.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"

namespace
{
    class RetryingGattClientConnectionTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<services::GattClientConnectionMock> connection;
        services::RetryingGattClientConnection retryingConnection{ connection };
        testing::StrictMock<services::GattClientUpdateObserverMock> updateObserver{ retryingConnection };

        const uint16_t handle = 0x1;
        std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
        infra::Function<void(services::GattResult)> onDoneMock;
    };
}

TEST_F(RetryingGattClientConnectionTest, should_call_read_characteristic)
{
    infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadMock;

    EXPECT_CALL(connection, Read(handle, testing::Ref(onReadMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.Read(handle, onReadMock));
}

TEST_F(RetryingGattClientConnectionTest, should_call_write_characteristic)
{
    EXPECT_CALL(connection, Write(handle, testing::ElementsAreArray(data), testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.Write(handle, data, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_not_retry_write_without_response_when_accepted)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.WriteWithoutResponse(handle, data));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_report_a_refused_write_without_response_instead_of_retrying)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    EXPECT_EQ(services::GattRequestStatus::invalidState, retryingConnection.WriteWithoutResponse(handle, data));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_report_an_unsupported_write_without_response)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::notSupported));

    EXPECT_EQ(services::GattRequestStatus::notSupported, retryingConnection.WriteWithoutResponse(handle, data));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_retry_write_without_response_while_busy)
{
    testing::InSequence sequence;

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.WriteWithoutResponse(handle, data));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_space_out_the_retries)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.WriteWithoutResponse(handle, data));

    ForwardTime(services::RetryingGattClientConnection::defaultRetryInterval / 2);

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    ForwardTime(services::RetryingGattClientConnection::defaultRetryInterval);
}

TEST_F(RetryingGattClientConnectionTest, should_stop_retrying_once_the_connection_refuses)
{
    testing::InSequence sequence;

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::invalidState));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.WriteWithoutResponse(handle, data));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_refuse_a_second_write_without_response_while_retrying)
{
    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.WriteWithoutResponse(handle, data));
    EXPECT_EQ(services::GattRequestStatus::busy, retryingConnection.WriteWithoutResponse(handle, data));

    EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, a_pending_retry_is_abandoned_when_the_decorator_is_destroyed)
{
    {
        services::RetryingGattClientConnection shortLived{ connection };

        EXPECT_CALL(connection, WriteWithoutResponse(handle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::busy));
        EXPECT_EQ(services::GattRequestStatus::accepted, shortLived.WriteWithoutResponse(handle, data));
    }

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(RetryingGattClientConnectionTest, should_call_enable_notification_characteristic)
{
    EXPECT_CALL(connection, EnableNotification(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.EnableNotification(handle, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_call_disable_notification_characteristic)
{
    EXPECT_CALL(connection, DisableNotification(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.DisableNotification(handle, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_call_enable_indication_characteristic)
{
    EXPECT_CALL(connection, EnableIndication(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.EnableIndication(handle, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_call_disable_indication_characteristic)
{
    EXPECT_CALL(connection, DisableIndication(handle, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.DisableIndication(handle, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_forward_notification_received)
{
    EXPECT_CALL(updateObserver, NotificationReceived(handle, testing::ElementsAreArray(data)));
    retryingConnection.NotificationReceived(handle, data);
}

TEST_F(RetryingGattClientConnectionTest, should_forward_indication_received)
{
    infra::VerifyingFunction<void()> onDone;

    EXPECT_CALL(updateObserver, IndicationReceived(handle, testing::ElementsAreArray(data), testing::_)).WillOnce(testing::InvokeArgument<2>());
    retryingConnection.IndicationReceived(handle, data, onDone);
}

TEST_F(RetryingGattClientConnectionTest, should_forward_read_blob)
{
    infra::Function<void(services::GattResult, infra::ConstByteRange)> onReadMock;

    EXPECT_CALL(connection, ReadBlob(handle, 0x20, testing::Ref(onReadMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.ReadBlob(handle, 0x20, onReadMock));
}

TEST_F(RetryingGattClientConnectionTest, should_forward_prepare_write)
{
    infra::Function<void(services::GattResult, uint16_t, infra::ConstByteRange)> onPrepareMock;

    EXPECT_CALL(connection, PrepareWrite(handle, 0x10, testing::ElementsAreArray(data), testing::Ref(onPrepareMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.PrepareWrite(handle, 0x10, data, onPrepareMock));
}

TEST_F(RetryingGattClientConnectionTest, should_forward_execute_write)
{
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::write, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.ExecuteWrite(services::GattExecuteWriteFlag::write, onDoneMock));
}

TEST_F(RetryingGattClientConnectionTest, should_forward_a_cancelling_execute_write)
{
    EXPECT_CALL(connection, ExecuteWrite(services::GattExecuteWriteFlag::cancel, testing::Ref(onDoneMock))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, retryingConnection.ExecuteWrite(services::GattExecuteWriteFlag::cancel, onDoneMock));
}
