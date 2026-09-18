#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/profile/NordicUartCentral.hpp"
#include "services/ble/profile/test_doubles/NordicUartMock.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "gmock/gmock.h"
#include <array>
#include <numeric>

namespace
{
    constexpr services::AttAttribute::Handle serviceHandle = 0x10;
    constexpr services::AttAttribute::Handle serviceEndHandle = 0x1F;
    constexpr services::AttAttribute::Handle rxHandle = 0x11;
    constexpr services::AttAttribute::Handle rxValueHandle = 0x12;
    constexpr services::AttAttribute::Handle txHandle = 0x13;
    constexpr services::AttAttribute::Handle txValueHandle = 0x14;

    constexpr uint16_t mtu = 40;
    constexpr std::size_t payloadSize = mtu - services::attValueHeaderSize;

    using PropertyFlags = services::GattCharacteristic::PropertyFlags;

    class NordicUartCentralTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        explicit NordicUartCentralTest(services::NordicUartCentral::WriteMode writeMode = services::NordicUartCentral::WriteMode::withResponse)
            : central(connection, writeMode)
        {
            observer.Attach(central);
        }

        void ReportService(const services::AttAttribute::Uuid& type)
        {
            services::GattService service{ type, serviceHandle, serviceEndHandle };

            connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([&service](auto& connectionObserver)
                {
                    connectionObserver.ServiceDiscovered(service);
                });
        }

        void ReportCharacteristic(const services::AttAttribute::Uuid& type, services::AttAttribute::Handle handle, services::AttAttribute::Handle valueHandle, PropertyFlags properties)
        {
            services::GattCharacteristic characteristic{ type, handle, valueHandle, properties };

            connection.infra::Subject<services::GattClientConnectionObserver>::NotifyObservers([&characteristic](auto& connectionObserver)
                {
                    connectionObserver.CharacteristicDiscovered(characteristic);
                });
        }

        void ReportNotification(infra::ConstByteRange data)
        {
            connection.infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([&data](auto& updateObserver)
                {
                    updateObserver.NotificationReceived(txValueHandle, data);
                });
        }

        void StartDiscovery()
        {
            EXPECT_CALL(connection, DiscoverServices(testing::_)).WillOnce([this](const infra::Function<void(services::GattResult)>& onDone)
                {
                    onServicesDiscovered = onDone;
                    return services::GattRequestStatus::accepted;
                });

            EXPECT_EQ(services::GattRequestStatus::accepted, central.Discover([this](services::GattResult result)
                                                                 {
                                                                     discoveryResult = result;
                                                                 }));
        }

        void DiscoverCharacteristics(PropertyFlags rxProperties = PropertyFlags::write | PropertyFlags::writeWithoutResponse)
        {
            EXPECT_CALL(connection, DiscoverCharacteristics(serviceHandle, serviceEndHandle, testing::_)).WillOnce([this](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
                {
                    onCharacteristicsDiscovered = onDone;
                    return services::GattRequestStatus::accepted;
                });

            onServicesDiscovered(services::GattResult::success);

            ReportCharacteristic(services::uuid::nordicUartRx, rxHandle, rxValueHandle, rxProperties);
            ReportCharacteristic(services::uuid::nordicUartTx, txHandle, txValueHandle, PropertyFlags::notify);
        }

        void Open(PropertyFlags rxProperties = PropertyFlags::write | PropertyFlags::writeWithoutResponse)
        {
            StartDiscovery();
            ReportService(services::uuid::nordicUartService);
            DiscoverCharacteristics(rxProperties);

            EXPECT_CALL(connection, EnableNotification(txValueHandle, testing::_)).WillOnce([this](auto, const infra::Function<void(services::GattResult)>& onDone)
                {
                    onNotificationEnabled = onDone;
                    return services::GattRequestStatus::accepted;
                });

            onCharacteristicsDiscovered(services::GattResult::success);

            EXPECT_CALL(observer, Opened());
            onNotificationEnabled(services::GattResult::success);

            EXPECT_EQ(services::GattResult::success, discoveryResult);
        }

        testing::StrictMock<services::GattClientConnectionMock> connection;
        services::NordicUartCentral central;
        testing::StrictMock<services::NordicUartObserverMock> observer;

        std::optional<services::GattResult> discoveryResult;
        infra::Function<void(services::GattResult)> onServicesDiscovered;
        infra::Function<void(services::GattResult)> onCharacteristicsDiscovered;
        infra::Function<void(services::GattResult)> onNotificationEnabled;
    };

    class NordicUartCentralWithoutResponseTest
        : public NordicUartCentralTest
    {
    public:
        NordicUartCentralWithoutResponseTest()
            : NordicUartCentralTest(services::NordicUartCentral::WriteMode::withoutResponse)
        {}
    };
}

TEST_F(NordicUartCentralTest, is_closed_before_discovery)
{
    EXPECT_FALSE(central.IsOpen());
}

TEST_F(NordicUartCentralTest, reports_unsupported_when_the_peer_does_not_expose_the_service)
{
    StartDiscovery();
    ReportService(services::AttAttribute::Uuid16{ 0x180A });

    onServicesDiscovered(services::GattResult::success);

    EXPECT_EQ(services::GattResult::unsupported, discoveryResult);
    EXPECT_FALSE(central.IsOpen());
}

TEST_F(NordicUartCentralTest, reports_unsupported_when_a_characteristic_is_missing)
{
    StartDiscovery();
    ReportService(services::uuid::nordicUartService);

    EXPECT_CALL(connection, DiscoverCharacteristics(serviceHandle, serviceEndHandle, testing::_)).WillOnce([this](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onCharacteristicsDiscovered = onDone;
            return services::GattRequestStatus::accepted;
        });

    onServicesDiscovered(services::GattResult::success);
    ReportCharacteristic(services::uuid::nordicUartRx, rxHandle, rxValueHandle, PropertyFlags::write);

    onCharacteristicsDiscovered(services::GattResult::success);

    EXPECT_EQ(services::GattResult::unsupported, discoveryResult);
}

TEST_F(NordicUartCentralTest, reports_a_failed_service_discovery_unchanged)
{
    StartDiscovery();

    onServicesDiscovered(services::GattResult::disconnected);

    EXPECT_EQ(services::GattResult::disconnected, discoveryResult);
}

TEST_F(NordicUartCentralTest, subscribing_to_tx_opens_the_pipe)
{
    Open();

    EXPECT_TRUE(central.IsOpen());
}

TEST_F(NordicUartCentralTest, a_refused_subscription_leaves_the_pipe_closed)
{
    StartDiscovery();
    ReportService(services::uuid::nordicUartService);
    DiscoverCharacteristics();

    EXPECT_CALL(connection, EnableNotification(txValueHandle, testing::_)).WillOnce([this](auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onNotificationEnabled = onDone;
            return services::GattRequestStatus::accepted;
        });

    onCharacteristicsDiscovered(services::GattResult::success);
    onNotificationEnabled(services::GattResult::notPermitted);

    EXPECT_EQ(services::GattResult::notPermitted, discoveryResult);
    EXPECT_FALSE(central.IsOpen());
}

TEST_F(NordicUartCentralTest, max_send_size_is_the_mtu_without_its_header)
{
    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(mtu));

    EXPECT_EQ(payloadSize, central.MaxSendSize());
}

TEST_F(NordicUartCentralTest, a_notification_on_tx_is_forwarded_to_the_receive_callback)
{
    Open();

    std::array<uint8_t, 3> data{ 0x0A, 0x0B, 0x0C };
    infra::ConstByteRange received;
    uint32_t receivedCount{ 0 };

    central.ReceiveData([&received, &receivedCount](infra::ConstByteRange data)
        {
            received = data;
            ++receivedCount;
        });

    ReportNotification(infra::MakeConstByteRange(data));

    EXPECT_EQ(1, receivedCount);
    EXPECT_THAT(received, testing::ElementsAreArray(data));
}

TEST_F(NordicUartCentralTest, send_writes_rx_with_a_response_by_default)
{
    Open();

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;
    infra::Function<void(services::GattResult)> onWritten;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(mtu));
    EXPECT_CALL(connection, Write(rxValueHandle, testing::ElementsAreArray(data), testing::_)).WillOnce([&onWritten](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onWritten = onDone;
            return services::GattRequestStatus::accepted;
        });

    central.SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    EXPECT_CALL(onSent, callback());
    onWritten(services::GattResult::success);
}

TEST_F(NordicUartCentralTest, send_splits_data_larger_than_one_write)
{
    Open();

    std::array<uint8_t, payloadSize + 5> data{};
    std::iota(data.begin(), data.end(), static_cast<uint8_t>(1));

    auto whole = infra::MakeConstByteRange(data);
    infra::MockCallback<void()> onSent;
    infra::Function<void(services::GattResult)> onWritten;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillRepeatedly(testing::Return(mtu));

    EXPECT_CALL(connection, Write(rxValueHandle, infra::ByteRangeContentsEqual(infra::Head(whole, payloadSize)), testing::_)).WillOnce([&onWritten](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onWritten = onDone;
            return services::GattRequestStatus::accepted;
        });

    central.SendData(whole, [&onSent]()
        {
            onSent.callback();
        });

    EXPECT_CALL(connection, Write(rxValueHandle, infra::ByteRangeContentsEqual(infra::DiscardHead(whole, payloadSize)), testing::_)).WillOnce([&onWritten](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onWritten = onDone;
            return services::GattRequestStatus::accepted;
        });

    onWritten(services::GattResult::success);

    EXPECT_CALL(onSent, callback());
    onWritten(services::GattResult::success);
}

TEST_F(NordicUartCentralTest, a_disconnection_during_a_send_closes_the_pipe)
{
    Open();

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;
    infra::Function<void(services::GattResult)> onWritten;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(mtu));
    EXPECT_CALL(connection, Write(rxValueHandle, testing::_, testing::_)).WillOnce([&onWritten](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onWritten = onDone;
            return services::GattRequestStatus::accepted;
        });

    central.SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    testing::InSequence sequence;
    EXPECT_CALL(onSent, callback());
    EXPECT_CALL(observer, Closed());

    onWritten(services::GattResult::disconnected);

    EXPECT_FALSE(central.IsOpen());
}

TEST_F(NordicUartCentralTest, a_refused_write_completes_the_send)
{
    Open();

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(mtu));
    EXPECT_CALL(connection, Write(rxValueHandle, testing::_, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::invalidState));
    EXPECT_CALL(onSent, callback());

    central.SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });
}

TEST_F(NordicUartCentralWithoutResponseTest, send_writes_rx_without_a_response_when_the_peer_offers_it)
{
    Open();

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillRepeatedly(testing::Return(mtu));
    EXPECT_CALL(connection, WriteWithoutResponse(rxValueHandle, testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_CALL(onSent, callback());

    central.SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    ExecuteAllActions();
}

TEST_F(NordicUartCentralWithoutResponseTest, send_falls_back_to_a_write_request_when_the_peer_does_not_offer_write_without_response)
{
    Open(PropertyFlags::write);

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;
    infra::Function<void(services::GattResult)> onWritten;

    EXPECT_CALL(connection, EffectiveMaxAttMtuSize()).WillOnce(testing::Return(mtu));
    EXPECT_CALL(connection, Write(rxValueHandle, testing::ElementsAreArray(data), testing::_)).WillOnce([&onWritten](auto, auto, const infra::Function<void(services::GattResult)>& onDone)
        {
            onWritten = onDone;
            return services::GattRequestStatus::accepted;
        });

    central.SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    EXPECT_CALL(onSent, callback());
    onWritten(services::GattResult::success);
}
