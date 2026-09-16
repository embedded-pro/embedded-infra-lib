#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/GapCentral.hpp"
#include "services/ble/test_doubles/GapCentralMock.hpp"
#include "services/ble/test_doubles/GapCentralObserverMock.hpp"
#include "gmock/gmock.h"
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
        EXPECT_CALL(gapObserver, StateChanged(GapState::connected));
        EXPECT_CALL(gapObserver, StateChanged(GapState::initiating));
        EXPECT_CALL(gapObserver, StateChanged(GapState::scanning));
        EXPECT_CALL(gapObserver, StateChanged(GapState::standby));

        gap.NotifyObservers([](GapCentralObserver& obs)
            {
                obs.StateChanged(GapState::connected);
                obs.StateChanged(GapState::initiating);
                obs.StateChanged(GapState::scanning);
                obs.StateChanged(GapState::standby);
            });
    }

    TEST_F(GapCentralDecoratorTest, forward_device_discovered_event_to_observers)
    {
        GapAdvertisingReport deviceDiscovered{ GapAdvertisingEventType::advInd, GapDeviceAddressType::publicAddress, hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, infra::BoundedVector<uint8_t>::WithMaxSize<gapMaxAdvertisementDataSize>{}, -75 };

        EXPECT_CALL(gapObserver, DeviceDiscovered(ObjectContentsEqual(deviceDiscovered)));

        gap.NotifyObservers([&deviceDiscovered](GapCentralObserver& obs)
            {
                obs.DeviceDiscovered(deviceDiscovered);
            });
    }

    TEST_F(GapCentralDecoratorTest, forward_resolve_private_address_to_subject)
    {
        EXPECT_CALL(gap, ResolvePrivateAddress(macAddress)).WillOnce(testing::Return(std::nullopt));
        EXPECT_EQ(decorator.ResolvePrivateAddress(macAddress), std::nullopt);

        EXPECT_CALL(gap, ResolvePrivateAddress(macAddress)).WillOnce(testing::Return(std::make_optional(macAddress)));
        EXPECT_EQ(decorator.ResolvePrivateAddress(macAddress), macAddress);
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Connect(MacAddressContentsEqual(macAddress), GapDeviceAddressType::publicAddress, infra::Duration{ 0 }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<3>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Connect(macAddress, GapDeviceAddressType::publicAddress, std::chrono::seconds(0), infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_timeout_result)
    {
        EXPECT_CALL(gap, Connect(MacAddressContentsEqual(macAddress), GapDeviceAddressType::publicAddress, infra::Duration{ 0 }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<3>(GapCentral::Result::timeout), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Connect(macAddress, GapDeviceAddressType::publicAddress, std::chrono::seconds(0), infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::timeout)));
    }

    TEST_F(GapCentralDecoratorTest, connect_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, Connect(MacAddressContentsEqual(macAddress), GapDeviceAddressType::publicAddress, infra::Duration{ 0 }, testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.Connect(macAddress, GapDeviceAddressType::publicAddress, std::chrono::seconds(0), RejectedCallback()));
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
        EXPECT_CALL(gap, SetAddress(MacAddressContentsEqual(macAddress), GapDeviceAddressType::randomAddress, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<2>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetAddress(macAddress, GapDeviceAddressType::randomAddress, infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, set_address_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, SetAddress(MacAddressContentsEqual(macAddress), GapDeviceAddressType::randomAddress, testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.SetAddress(macAddress, GapDeviceAddressType::randomAddress, RejectedCallback()));
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_forwards_request_and_result)
    {
        EXPECT_CALL(gap, StartDeviceDiscovery(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapCentral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.StartDeviceDiscovery(infra::VerifyingFunction<void(GapCentral::Result)>(GapCentral::Result::success)));
    }

    TEST_F(GapCentralDecoratorTest, start_device_discovery_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, StartDeviceDiscovery(testing::_)).WillOnce(testing::Return(GapRequestStatus::busy));

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
}
