#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/profile/NordicUartPeripheral.hpp"
#include "services/ble/profile/test_doubles/NordicUartMock.hpp"
#include "services/ble/test_doubles/GattServerMock.hpp"
#include "gmock/gmock.h"
#include <algorithm>
#include <array>
#include <numeric>
#include <optional>

namespace
{
    constexpr std::size_t defaultPayloadSize = services::attDefaultMaxMtuSize - services::attValueHeaderSize;

    class NordicUartPeripheralTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        NordicUartPeripheralTest()
        {
            EXPECT_CALL(gattServer, AddService(testing::_));
            peripheral.emplace(gattServer);

            for (auto& characteristic : peripheral->Service().Characteristics())
                characteristic.Attach(operations);

            observer.Attach(*peripheral);
        }

        services::GattServerCharacteristic& Characteristic(const services::AttAttribute::Uuid& type)
        {
            auto& characteristics = peripheral->Service().Characteristics();
            auto found = std::find_if(characteristics.begin(), characteristics.end(), [&type](const auto& characteristic)
                {
                    return characteristic.Type() == type;
                });

            really_assert(found != characteristics.end());
            return *found;
        }

        void Open()
        {
            EXPECT_CALL(observer, Opened());
            peripheral->NotificationsEnabled(true);
        }

        void PeerWrites(infra::ConstByteRange data)
        {
            Characteristic(services::uuid::nordicUartRx).NotifyObservers([&data](auto& characteristicObserver)
                {
                    characteristicObserver.DataReceived(data);
                });
        }

        testing::StrictMock<services::GattServerMock> gattServer;
        testing::StrictMock<services::GattServerCharacteristicOperationsMock> operations;
        std::optional<services::NordicUartPeripheral> peripheral;
        testing::StrictMock<services::NordicUartObserverMock> observer;
    };
}

TEST_F(NordicUartPeripheralTest, exposes_the_nordic_uart_service_with_rx_and_write_and_tx_notify)
{
    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::nordicUartService }, peripheral->Service().Type());

    auto& rx = Characteristic(services::uuid::nordicUartRx);
    auto& tx = Characteristic(services::uuid::nordicUartTx);

    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::write | services::GattCharacteristic::PropertyFlags::writeWithoutResponse, rx.Properties());
    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::notify, tx.Properties());
}

TEST_F(NordicUartPeripheralTest, is_closed_until_notifications_are_enabled)
{
    EXPECT_FALSE(peripheral->IsOpen());
}

TEST_F(NordicUartPeripheralTest, opens_when_notifications_are_enabled)
{
    Open();

    EXPECT_TRUE(peripheral->IsOpen());
}

TEST_F(NordicUartPeripheralTest, enabling_notifications_twice_reports_opened_once)
{
    Open();
    peripheral->NotificationsEnabled(true);
}

TEST_F(NordicUartPeripheralTest, closes_when_notifications_are_disabled)
{
    Open();

    EXPECT_CALL(observer, Closed());
    peripheral->NotificationsEnabled(false);

    EXPECT_FALSE(peripheral->IsOpen());
}

TEST_F(NordicUartPeripheralTest, max_send_size_is_the_default_mtu_without_its_header)
{
    EXPECT_EQ(defaultPayloadSize, peripheral->MaxSendSize());
}

TEST_F(NordicUartPeripheralTest, max_send_size_follows_the_negotiated_mtu)
{
    EXPECT_CALL(gattServer, AddService(testing::_));
    services::NordicUartPeripheral bounded{ gattServer, 247 };

    bounded.MaxAttMtuSizeChanged(128);

    EXPECT_EQ(128 - services::attValueHeaderSize, bounded.MaxSendSize());
}

TEST_F(NordicUartPeripheralTest, a_negotiation_cannot_raise_the_payload_above_what_was_configured)
{
    peripheral->MaxAttMtuSizeChanged(247);

    EXPECT_EQ(defaultPayloadSize, peripheral->MaxSendSize());
}

TEST_F(NordicUartPeripheralTest, max_send_size_is_capped_by_the_configured_mtu)
{
    EXPECT_CALL(gattServer, AddService(testing::_));
    services::NordicUartPeripheral bounded{ gattServer, 64 };

    bounded.MaxAttMtuSizeChanged(247);

    EXPECT_EQ(64 - services::attValueHeaderSize, bounded.MaxSendSize());
}

TEST_F(NordicUartPeripheralTest, send_notifies_data_that_fits_one_notification)
{
    Open();

    std::array<uint8_t, 4> data{ 0x01, 0x02, 0x03, 0x04 };
    infra::MockCallback<void()> onSent;

    EXPECT_CALL(operations, Update(testing::Ref(Characteristic(services::uuid::nordicUartTx)), testing::ElementsAreArray(data))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_CALL(onSent, callback());

    peripheral->SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    ExecuteAllActions();
}

TEST_F(NordicUartPeripheralTest, send_splits_data_larger_than_one_notification)
{
    Open();

    std::array<uint8_t, defaultPayloadSize + 5> data{};
    std::iota(data.begin(), data.end(), static_cast<uint8_t>(1));

    auto whole = infra::MakeConstByteRange(data);
    infra::MockCallback<void()> onSent;

    testing::InSequence sequence;
    EXPECT_CALL(operations, Update(testing::_, infra::ByteRangeContentsEqual(infra::Head(whole, defaultPayloadSize)))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_CALL(operations, Update(testing::_, infra::ByteRangeContentsEqual(infra::DiscardHead(whole, defaultPayloadSize)))).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_CALL(onSent, callback());

    peripheral->SendData(whole, [&onSent]()
        {
            onSent.callback();
        });

    ExecuteAllActions();
}

TEST_F(NordicUartPeripheralTest, send_of_nothing_completes_without_notifying)
{
    Open();

    infra::MockCallback<void()> onSent;
    EXPECT_CALL(onSent, callback());

    peripheral->SendData(infra::ConstByteRange{}, [&onSent]()
        {
            onSent.callback();
        });

    ExecuteAllActions();
}

TEST_F(NordicUartPeripheralTest, send_completes_when_notifications_are_disabled_halfway)
{
    Open();

    std::array<uint8_t, defaultPayloadSize + 5> data{};
    infra::MockCallback<void()> onSent;

    EXPECT_CALL(operations, Update(testing::_, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    peripheral->SendData(infra::MakeConstByteRange(data), [&onSent]()
        {
            onSent.callback();
        });

    testing::InSequence sequence;
    EXPECT_CALL(onSent, callback());
    EXPECT_CALL(observer, Closed());

    peripheral->NotificationsEnabled(false);

    ExecuteAllActions();
}

TEST_F(NordicUartPeripheralTest, a_write_to_rx_is_forwarded_to_the_receive_callback)
{
    std::array<uint8_t, 3> data{ 0x0A, 0x0B, 0x0C };
    infra::ConstByteRange received;
    uint32_t receivedCount{ 0 };

    peripheral->ReceiveData([&received, &receivedCount](infra::ConstByteRange data)
        {
            received = data;
            ++receivedCount;
        });

    PeerWrites(infra::MakeConstByteRange(data));

    EXPECT_EQ(1, receivedCount);
    EXPECT_THAT(received, testing::ElementsAreArray(data));
}

TEST_F(NordicUartPeripheralTest, a_write_to_rx_without_a_receive_callback_is_discarded)
{
    std::array<uint8_t, 3> data{ 0x0A, 0x0B, 0x0C };

    PeerWrites(infra::MakeConstByteRange(data));
}
