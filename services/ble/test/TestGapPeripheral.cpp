#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/GapPeripheral.hpp"
#include "services/ble/test_doubles/GapPeripheralMock.hpp"
#include "services/ble/test_doubles/GapPeripheralObserverMock.hpp"
#include "gmock/gmock.h"

namespace services
{
    namespace
    {
        class GapPeripheralDecoratorTest
            : public testing::Test
        {
        public:
            testing::StrictMock<GapPeripheralMock> gap;
            GapPeripheralDecorator decorator{ gap };
            testing::StrictMock<GapPeripheralObserverMock> gapObserver{ decorator };

            std::array<uint8_t, 6> data{ 0, 1, 2, 3, 4, 5 };
            GapConnectionParameters connectionParameters{ 10, 20, 30, 40 };
            testing::StrictMock<infra::MockCallback<void(GapPeripheral::Result)>> onDoneNotExpected;

            infra::Function<void(GapPeripheral::Result)> RejectedCallback()
            {
                return [this](GapPeripheral::Result result)
                {
                    onDoneNotExpected.callback(result);
                };
            }
        };
    }

    TEST_F(GapPeripheralDecoratorTest, forward_all_events_to_observers)
    {
        EXPECT_CALL(gapObserver, StateChanged(GapPeripheralState::connected));
        EXPECT_CALL(gapObserver, StateChanged(GapPeripheralState::advertising));

        gap.NotifyObservers([](GapPeripheralObserver& obs)
            {
                obs.StateChanged(GapPeripheralState::connected);
                obs.StateChanged(GapPeripheralState::advertising);
            });
    }

    TEST_F(GapPeripheralDecoratorTest, forward_all_getters_to_subject)
    {
        GapAddress address{ hal::MacAddress({ 5, 4, 3, 2, 1, 0 }), GapDeviceAddressType::publicAddress };
        EXPECT_CALL(gap, GetAddress()).WillOnce(testing::Return(address));
        EXPECT_THAT(decorator.GetAddress(), testing::Eq(address));

        GapAddress identityAddress{ hal::MacAddress({ 0, 1, 2, 3, 4, 5 }), GapDeviceAddressType::publicAddress };
        EXPECT_CALL(gap, GetIdentityAddress()).WillOnce(testing::Return(identityAddress));
        EXPECT_THAT(decorator.GetIdentityAddress(), testing::Eq(identityAddress));

        EXPECT_CALL(gap, GetAdvertisementData()).WillOnce(testing::Return(infra::MakeConstByteRange(data)));
        EXPECT_THAT(decorator.GetAdvertisementData(), infra::ContentsEqual(data));

        EXPECT_CALL(gap, GetScanResponseData()).WillOnce(testing::Return(infra::MakeConstByteRange(data)));
        EXPECT_THAT(decorator.GetScanResponseData(), infra::ContentsEqual(data));
    }

    TEST_F(GapPeripheralDecoratorTest, set_advertisement_data_forwards_request_and_result)
    {
        EXPECT_CALL(gap, SetAdvertisementData(infra::ContentsEqual(data), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetAdvertisementData(data, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, set_advertisement_data_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, SetAdvertisementData(infra::ContentsEqual(data), testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.SetAdvertisementData(data, RejectedCallback()));
    }

    TEST_F(GapPeripheralDecoratorTest, set_scan_response_data_forwards_request_and_result)
    {
        EXPECT_CALL(gap, SetScanResponseData(infra::ContentsEqual(data), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPeripheral::Result::controllerError), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetScanResponseData(data, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::controllerError)));
    }

    TEST_F(GapPeripheralDecoratorTest, set_scan_response_data_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, SetScanResponseData(infra::ContentsEqual(data), testing::_)).WillOnce(testing::Return(GapRequestStatus::busy));

        EXPECT_EQ(GapRequestStatus::busy, decorator.SetScanResponseData(data, RejectedCallback()));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Advertise(GapAdvertisingParameters{ GapAdvertisementType::advNonconnInd, 32, GapAdvertisingChannels::all, GapAdvertisingFilterPolicy::any }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Advertise(GapAdvertisementType::advNonconnInd, 32, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, Advertise(GapAdvertisingParameters{ GapAdvertisementType::advInd, 16, GapAdvertisingChannels::all, GapAdvertisingFilterPolicy::any }, testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidParameter));

        EXPECT_EQ(GapRequestStatus::invalidParameter, decorator.Advertise(GapAdvertisementType::advInd, 16, RejectedCallback()));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_forwards_the_channel_map_and_filter_policy_it_is_given)
    {
        const GapAdvertisingParameters parameters{ GapAdvertisementType::advInd, 0x0040u, GapAdvertisingChannels::channel37, GapAdvertisingFilterPolicy::filterScanAndConnectionRequests };

        EXPECT_CALL(gap, Advertise(parameters, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Advertise(parameters, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, the_default_advertising_parameters_are_within_their_range)
    {
        EXPECT_GE(GapPeripheral::defaultAdvertisingParameters.interval, GapAdvertisingParameters::intervalMultiplierMin);
        EXPECT_LE(GapPeripheral::defaultAdvertisingParameters.interval, GapAdvertisingParameters::intervalMultiplierMax);
        EXPECT_EQ(GapAdvertisingChannels::all, GapPeripheral::defaultAdvertisingParameters.channels);
        EXPECT_EQ(GapAdvertisingFilterPolicy::any, GapPeripheral::defaultAdvertisingParameters.filterPolicy);
    }

    TEST_F(GapPeripheralDecoratorTest, standby_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Standby(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Standby(infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, standby_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, Standby(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.Standby(RejectedCallback()));
    }

    TEST_F(GapPeripheralDecoratorTest, set_connection_parameters_forwards_request_and_result)
    {
        EXPECT_CALL(gap, RequestConnectionParameterUpdate(testing::_, testing::_))
            .WillOnce(testing::DoAll(testing::Invoke([this](const GapConnectionParameters& param, const infra::Function<void(GapPeripheral::Result)>&)
                                         {
                                             EXPECT_EQ(connectionParameters.minConnectionInterval, param.minConnectionInterval);
                                             EXPECT_EQ(connectionParameters.maxConnectionInterval, param.maxConnectionInterval);
                                             EXPECT_EQ(connectionParameters.peripheralLatency, param.peripheralLatency);
                                             EXPECT_EQ(connectionParameters.supervisionTimeout, param.supervisionTimeout);
                                         }),
                testing::InvokeArgument<1>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.RequestConnectionParameterUpdate(connectionParameters, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, set_connection_parameters_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gap, RequestConnectionParameterUpdate(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.RequestConnectionParameterUpdate(connectionParameters, RejectedCallback()));
    }

    TEST(GapPeripheralInsertionOperatorStateTest, state_overload_operator)
    {
        infra::StringOutputStream::WithStorage<128> stream;

        stream << GapPeripheralState::standby << " " << GapPeripheralState::advertising << " " << GapPeripheralState::connected;

        EXPECT_EQ("Standby Advertising Connected", stream.Storage());
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_scannable_undirected_forwards_request_and_result)
    {
        EXPECT_CALL(gap, Advertise(GapAdvertisingParameters{ GapAdvertisementType::advScanInd, 0x20, GapAdvertisingChannels::all, GapAdvertisingFilterPolicy::any }, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.Advertise(GapAdvertisementType::advScanInd, 0x20, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_directed_at_a_peer_forwards_request_and_result)
    {
        const GapAddress peer{ hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, GapDeviceAddressType::publicAddress };

        EXPECT_CALL(gap, AdvertiseDirected(GapDirectedAdvertisementType::lowDutyCycle, peer, 0x20, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<3>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.AdvertiseDirected(GapDirectedAdvertisementType::lowDutyCycle, peer, 0x20, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_directed_at_high_duty_cycle)
    {
        // High duty cycle directed advertising is the standard mechanism for fast reconnection to a
        // known peer.
        const GapAddress peer{ hal::MacAddress{ 5, 4, 3, 2, 1, 0 }, GapDeviceAddressType::randomAddress };

        EXPECT_CALL(gap, AdvertiseDirected(GapDirectedAdvertisementType::highDutyCycle, peer, testing::_, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<3>(GapPeripheral::Result::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.AdvertiseDirected(GapDirectedAdvertisementType::highDutyCycle, peer, 0x20, infra::VerifyingFunction<void(GapPeripheral::Result)>(GapPeripheral::Result::success)));
    }

    TEST_F(GapPeripheralDecoratorTest, advertise_directed_forwards_rejection_without_invoking_callback)
    {
        const GapAddress peer{ hal::MacAddress{ 0, 1, 2, 3, 4, 5 }, GapDeviceAddressType::publicAddress };

        EXPECT_CALL(gap, AdvertiseDirected(GapDirectedAdvertisementType::lowDutyCycle, peer, 0x20, testing::_))
            .WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.AdvertiseDirected(GapDirectedAdvertisementType::lowDutyCycle, peer, 0x20, RejectedCallback()));
    }
}
