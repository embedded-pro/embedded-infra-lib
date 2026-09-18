#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/ble/LinkConfiguringGapCentral.hpp"
#include "services/ble/test_doubles/GapCentralMock.hpp"
#include "services/ble/test_doubles/GapCentralObserverMock.hpp"
#include "gmock/gmock.h"

namespace services
{
    namespace
    {
        class LinkConfiguringGapCentralTest
            : public testing::Test
            , public infra::ClockFixture
        {
        public:
            testing::StrictMock<GapCentralMock> gap;
            LinkConfiguringGapCentral linkConfiguring{ gap };
            testing::StrictMock<GapCentralObserverMock> observer{ linkConfiguring };

            const GapDataLength maximumDataLength{ GapDataLength::Maximum(GapPhy::le1M) };

            void Connect()
            {
                EXPECT_CALL(observer, StateChanged(GapCentralState::connected));
                gap.ChangeState(GapCentralState::connected);
            }

            void Disconnect()
            {
                EXPECT_CALL(observer, StateChanged(GapCentralState::standby));
                gap.ChangeState(GapCentralState::standby);
            }

            void ReportPhy(GapPhy txPhy = GapPhy::le2M, GapPhy rxPhy = GapPhy::le2M)
            {
                EXPECT_CALL(observer, PhyUpdated(txPhy, rxPhy));
                gap.ChangePhy(txPhy, rxPhy);
            }

            void Settle()
            {
                ForwardTime(std::chrono::milliseconds(1));
            }
        };
    }

    TEST_F(LinkConfiguringGapCentralTest, raises_the_phy_when_a_connection_is_established)
    {
        Connect();

        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, does_not_ask_from_within_the_notification_that_prompted_it)
    {
        EXPECT_CALL(gap, SetPhy(testing::_, testing::_)).Times(0);
        EXPECT_CALL(gap, SetDataLength(testing::_)).Times(0);

        Connect();
        ReportPhy();

        testing::Mock::VerifyAndClearExpectations(&gap);

        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        EXPECT_CALL(gap, SetDataLength(maximumDataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, sets_the_data_length_once_the_link_layer_reports_its_phy)
    {
        Connect();
        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        ReportPhy();

        EXPECT_CALL(gap, SetDataLength(maximumDataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, sets_the_data_length_straight_away_when_the_controller_refuses_the_phy)
    {
        Connect();

        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::notSupported));
        EXPECT_CALL(gap, SetDataLength(maximumDataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, follows_a_phy_the_peer_asked_for)
    {
        Connect();
        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        ReportPhy();
        EXPECT_CALL(gap, SetDataLength(maximumDataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        // Nothing was asked for this time; the peer moved the link back to LE 1M.
        ReportPhy(GapPhy::le1M, GapPhy::le1M);
        EXPECT_CALL(gap, SetDataLength(maximumDataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, forwards_a_data_length_change_without_acting_on_it)
    {
        const GapDataLength negotiated{ 27, 328 };

        EXPECT_CALL(observer, DataLengthChanged(negotiated));
        gap.ChangeDataLength(negotiated);

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, leaves_the_link_alone_in_every_other_state)
    {
        EXPECT_CALL(observer, StateChanged(GapCentralState::scanning));
        EXPECT_CALL(observer, StateChanged(GapCentralState::initiating));

        gap.ChangeState(GapCentralState::scanning);
        gap.ChangeState(GapCentralState::initiating);
        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, drops_a_pending_request_when_the_connection_goes)
    {
        Connect();
        Disconnect();

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, drops_a_pending_data_length_request_when_the_connection_goes)
    {
        Connect();
        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        ReportPhy();
        Disconnect();

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralTest, configures_the_link_again_on_the_next_connection)
    {
        Connect();
        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        Disconnect();
        Connect();

        EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();
    }

    TEST(LinkConfiguringGapCentralConfigurationTest, applies_the_phy_and_data_length_it_is_configured_with)
    {
        infra::ClockFixture clock;
        testing::StrictMock<GapCentralMock> gap;
        const LinkConfiguringGapCentral::Configuration configuration{ GapPhy::le1M, GapPhy::le1M, GapDataLength{ 100, 1000 } };
        LinkConfiguringGapCentral linkConfiguring{ gap, configuration };

        EXPECT_CALL(gap, SetPhy(GapPhy::le1M, GapPhy::le1M)).WillOnce(testing::Return(GapRequestStatus::accepted));
        gap.ChangeState(GapCentralState::connected);
        clock.ForwardTime(std::chrono::milliseconds(1));

        EXPECT_CALL(gap, SetDataLength(configuration.dataLength)).WillOnce(testing::Return(GapRequestStatus::accepted));
        gap.ChangePhy(GapPhy::le1M, GapPhy::le1M);
        clock.ForwardTime(std::chrono::milliseconds(1));
    }

    TEST(LinkConfiguringGapCentralConfigurationTest, asks_for_the_longest_payload_on_the_slowest_phy_by_default)
    {
        EXPECT_EQ(GapPhy::le2M, LinkConfiguringGapCentral::defaultConfiguration.txPhy);
        EXPECT_EQ(GapPhy::le2M, LinkConfiguringGapCentral::defaultConfiguration.rxPhy);
        EXPECT_EQ(GapDataLength::initialMaxTxOctets, LinkConfiguringGapCentral::defaultConfiguration.dataLength.maxTxOctets);
        EXPECT_EQ(GapDataLength::InitialMaxTxTime(GapPhy::le1M), LinkConfiguringGapCentral::defaultConfiguration.dataLength.maxTxTime);
    }
}
