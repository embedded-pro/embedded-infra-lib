#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/GapCentral.hpp"
#include "services/ble/test_doubles/GapCentralMock.hpp"
#include "services/ble/test_doubles/GapCentralObserverMock.hpp"
#include "gmock/gmock.h"
#include <array>
#include <chrono>

namespace services
{
    namespace
    {
        class GapCentralDecoratorTest
            : public testing::Test
        {
        public:
            testing::StrictMock<GapCentralMock> gap;
            GapCentralDecorator decorator{ gap };
            testing::StrictMock<GapCentralObserverMock> gapObserver{ decorator };

            hal::MacAddress macAddress{ 0, 1, 2, 3, 4, 5 };
            testing::StrictMock<infra::MockCallback<void(GapCentral::Result)>> onDoneNotExpected;

            infra::Function<void(GapCentral::Result)> RejectedCallback()
            {
                return [this](GapCentral::Result result)
                {
                    onDoneNotExpected.callback(result);
                };
            }
        };
    }

    MATCHER_P(MacAddressContentsEqual, x, negation ? "Contents not equal" : "Contents are equal")
    {
        return infra::ContentsEqual(infra::MakeRange(x), infra::MakeRange(arg));
    }

    MATCHER_P(ObjectContentsEqual, x, negation ? "Contents not equal" : "Contents are equal")
    {
        return x.eventType == arg.eventType && x.addressType == arg.addressType && x.address == arg.address && x.rssi == arg.rssi;
    }

    TEST_F(GapCentralDecoratorTest, forward_all_state_changed_events_to_observers)
    {
        EXPECT_CALL(gapObserver, StateChanged(GapCentralState::connected));
        EXPECT_CALL(gapObserver, StateChanged(GapCentralState::initiating));
        EXPECT_CALL(gapObserver, StateChanged(GapCentralState::scanning));
        EXPECT_CALL(gapObserver, StateChanged(GapCentralState::standby));

        gap.NotifyObservers([](GapCentralObserver& obs)
            {
                obs.StateChanged(GapCentralState::connected);
                obs.StateChanged(GapCentralState::initiating);
                obs.StateChanged(GapCentralState::scanning);
                obs.StateChanged(GapCentralState::standby);
            });
    }

    TEST_F(GapCentralDecoratorTest, forward_device_discovered_event_to_observers)
    {
        GapAdvertisingReport deviceDiscovered{ GapAdvertisingEventType::advInd, GapDeviceAddressType::publicAddress, hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, infra::ConstByteRange(), -75 };

        EXPECT_CALL(gapObserver, DeviceDiscovered(ObjectContentsEqual(deviceDiscovered)));

        gap.NotifyObservers([&deviceDiscovered](GapCentralObserver& obs)
            {
                obs.DeviceDiscovered(deviceDiscovered);
            });
    }

    TEST_F(GapCentralDecoratorTest, forwards_a_report_that_views_its_advertising_data)
    {
        const std::array<uint8_t, 5> payload{ 0x02, 0x01, 0x06, 0x00, 0xFF };
        GapAdvertisingReport report{ GapAdvertisingEventType::advInd, GapDeviceAddressType::publicAddress,
            hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, infra::MakeConstByteRange(payload), -75 };

        EXPECT_CALL(gapObserver, DeviceDiscovered(testing::_)).WillOnce(testing::Invoke([&payload](const GapAdvertisingReport& forwarded)
            {
                // The view reaches the observer intact rather than being truncated into a
                // fixed-size member on the way.
                EXPECT_TRUE(infra::ContentsEqual(infra::MakeConstByteRange(payload), forwarded.data));
            }));

        gap.NotifyObservers([&report](GapCentralObserver& observer)
            {
                observer.DeviceDiscovered(report);
            });
    }

    TEST_F(GapCentralDecoratorTest, carries_an_rssi_of_127_as_not_available)
    {
        GapAdvertisingReport report{ GapAdvertisingEventType::advInd, GapDeviceAddressType::publicAddress,
            hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, infra::ConstByteRange(), gapRssiNotAvailable };

        EXPECT_CALL(gapObserver, DeviceDiscovered(testing::_)).WillOnce(testing::Invoke([](const GapAdvertisingReport& forwarded)
            {
                EXPECT_EQ(gapRssiNotAvailable, forwarded.rssi);
            }));

        gap.NotifyObservers([&report](GapCentralObserver& observer)
            {
                observer.DeviceDiscovered(report);
            });
    }

    TEST_F(GapCentralDecoratorTest, carries_the_whole_signed_rssi_range)
    {
        // Specification RSSI is signed 8-bit; -127 is the far end of it and the value an
        // unsigned or narrowed type would mangle.
        GapAdvertisingReport report{ GapAdvertisingEventType::advInd, GapDeviceAddressType::publicAddress,
            hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, infra::ConstByteRange(), -127 };

        EXPECT_CALL(gapObserver, DeviceDiscovered(testing::_)).WillOnce(testing::Invoke([](const GapAdvertisingReport& forwarded)
            {
                EXPECT_EQ(-127, forwarded.rssi);
            }));

        gap.NotifyObservers([&report](GapCentralObserver& observer)
            {
                observer.DeviceDiscovered(report);
            });
    }

    TEST_F(GapCentralDecoratorTest, forward_resolve_private_address_to_subject)
    {
        const GapAddress identity{ macAddress, GapDeviceAddressType::publicAddress };

        EXPECT_CALL(gap, ResolvePrivateAddress(macAddress)).WillOnce(testing::Return(std::nullopt));
        EXPECT_EQ(decorator.ResolvePrivateAddress(macAddress), std::nullopt);

        EXPECT_CALL(gap, ResolvePrivateAddress(macAddress)).WillOnce(testing::Return(std::make_optional(identity)));

        auto resolved = decorator.ResolvePrivateAddress(macAddress);

        ASSERT_TRUE(resolved);
        EXPECT_EQ(macAddress, resolved->address);
        // The kind of identity address it resolved to, which the bare address could not carry.
        EXPECT_EQ(GapDeviceAddressType::publicAddress, resolved->type);
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, infra::Duration{ 0 }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<2>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, std::chrono::seconds(0), infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_timeout_result)
    {
        EXPECT_CALL(gap, Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, infra::Duration{ 0 }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<2>(GapCentral::Result::timeout), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, std::chrono::seconds(0), infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::timeout)));
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, infra::Duration{ 0 }, testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, std::chrono::seconds(0), RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, cancel_connect_forwards_request_and_result)
    {
        EXPECT_CALL(gap, CancelConnect(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapCentral::Result::cancelled), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.CancelConnect(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::cancelled)));
    }

    TEST_F(GapCentralDecoratorTest, cancel_connect_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, CancelConnect(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.CancelConnect(RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, disconnect_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Disconnect(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Disconnect(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, disconnect_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, Disconnect(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.Disconnect(RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, set_address_forwards_request_and_result)
    {
        EXPECT_CALL(gap, SetAddress(GapAddress{ macAddress, GapDeviceAddressType::randomAddress }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetAddress(GapAddress{ macAddress, GapDeviceAddressType::randomAddress }, infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, set_address_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, SetAddress(GapAddress{ macAddress, GapDeviceAddressType::randomAddress }, testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.SetAddress(GapAddress{ macAddress, GapDeviceAddressType::randomAddress }, RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_forwards_request_and_result)
    {
        EXPECT_CALL(gap, StartDeviceDiscovery(testing::_, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StartDeviceDiscovery(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, StartDeviceDiscovery(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::busy));

        EXPECT_EQ(GapRequestStatus::busy, decorator.StartDeviceDiscovery(RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, stop_device_discovery_forwards_request_and_result)
    {
        EXPECT_CALL(gap, StopDeviceDiscovery(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StopDeviceDiscovery(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, stop_device_discovery_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, StopDeviceDiscovery(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.StopDeviceDiscovery(RejectedCallback()));
    }

    TEST(GapCentralInsertionOperatorStateTest, state_overload_operator)
    {
        infra::StringOutputStream::WithStorage<128> stream;

        stream << GapCentralState::standby << " " << GapCentralState::scanning << " " << GapCentralState::initiating << " " << GapCentralState::connected;

        EXPECT_EQ("Standby Scanning Initiating Connected", stream.Storage());
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_with_the_given_scan_parameters)
    {
        const GapScanParameters parameters{ 0x0100, 0x0050, GapScanType::passive };

        EXPECT_CALL(gap, StartDeviceDiscovery(parameters, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StartDeviceDiscovery(parameters, infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_without_parameters_uses_the_default)
    {
        // The one-argument overload is the abstraction's documented default rather than whatever
        // a port would otherwise pick, and it must reach the subject as real parameters.
        EXPECT_CALL(gap, StartDeviceDiscovery(GapCentral::defaultScanParameters, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StartDeviceDiscovery(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, start_a_passive_device_discovery)
    {
        // Passive scanning is how a central declines to send scan requests, which it could not
        // express at all before.
        const GapScanParameters passive{ 0x0010, 0x0010, GapScanType::passive };

        EXPECT_CALL(gap, StartDeviceDiscovery(passive, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StartDeviceDiscovery(passive, infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, scan_parameters_document_their_specification_range)
    {
        EXPECT_EQ(0x0004u, GapScanParameters::intervalMultiplierMin);
        EXPECT_EQ(0x4000u, GapScanParameters::intervalMultiplierMax);

        // The default must itself be inside the range it documents, and its window must not
        // exceed its interval.
        EXPECT_GE(GapCentral::defaultScanParameters.interval, GapScanParameters::intervalMultiplierMin);
        EXPECT_LE(GapCentral::defaultScanParameters.interval, GapScanParameters::intervalMultiplierMax);
        EXPECT_LE(GapCentral::defaultScanParameters.window, GapCentral::defaultScanParameters.interval);
    }
}
