#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/PostAssign.hpp"
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
            , public infra::EventDispatcherFixture
        {
        public:
            testing::StrictMock<GapCentralMock> gap;
            LinkConfiguringGapCentral linkConfiguring{ gap };
            testing::StrictMock<GapCentralObserverMock> observer{ linkConfiguring };

            const GapDataLength maximumDataLength{ GapDataLength::Maximum(GapPhy::le1M) };
            infra::Function<void(GapCentral::Result)> onStepDone;

            void ExpectSetPhy(GapRequestStatus status = GapRequestStatus::accepted)
            {
                EXPECT_CALL(gap, SetPhy(GapPhy::le2M, GapPhy::le2M, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<2>(&onStepDone), testing::Return(status)));
            }

            void ExpectSetDataLength(GapRequestStatus status = GapRequestStatus::accepted)
            {
                EXPECT_CALL(gap, SetDataLength(maximumDataLength, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<1>(&onStepDone), testing::Return(status)));
            }

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

            // A port holds a completion in an infra::AutoResetFunction, which releases it before
            // invoking it.
            void CompleteStep(GapCentral::Result result = GapCentral::Result::success)
            {
                infra::PostAssign(onStepDone, nullptr)(result);
            }
        };
    }

    TEST_F(LinkConfiguringGapCentralTest, sets_the_phy_when_a_connection_is_established)
    {
        Connect();

        ExpectSetPhy();
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, sets_the_data_length_only_after_the_phy_has_settled)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        ExpectSetDataLength();
        CompleteStep();
    }

    TEST_F(LinkConfiguringGapCentralTest, ends_after_the_data_length)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        ExpectSetDataLength();
        CompleteStep();

        CompleteStep();
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, sets_the_data_length_even_when_the_phy_could_not_be_raised)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        ExpectSetDataLength();
        CompleteStep(GapCentral::Result::controllerError);
    }

    TEST_F(LinkConfiguringGapCentralTest, moves_on_when_the_controller_refuses_the_phy_procedure)
    {
        Connect();

        ExpectSetPhy(GapRequestStatus::notSupported);
        ExpectSetDataLength();
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, ends_when_the_controller_refuses_the_data_length_procedure)
    {
        Connect();

        ExpectSetPhy(GapRequestStatus::notSupported);
        ExpectSetDataLength(GapRequestStatus::invalidState);
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, discards_a_step_completion_that_arrives_after_the_connection_is_gone)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        Disconnect();

        CompleteStep();
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, discards_a_scheduled_start_when_the_connection_is_gone_before_it_runs)
    {
        Connect();
        Disconnect();

        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, leaves_the_link_alone_in_every_other_state)
    {
        EXPECT_CALL(observer, StateChanged(GapCentralState::standby));
        EXPECT_CALL(observer, StateChanged(GapCentralState::scanning));
        EXPECT_CALL(observer, StateChanged(GapCentralState::initiating));

        gap.ChangeState(GapCentralState::standby);
        gap.ChangeState(GapCentralState::scanning);
        gap.ChangeState(GapCentralState::initiating);
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, configures_the_link_again_on_the_next_connection)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        ExpectSetDataLength();
        CompleteStep();
        CompleteStep();

        Disconnect();
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();
    }

    TEST_F(LinkConfiguringGapCentralTest, skips_a_connection_whose_predecessor_never_reported)
    {
        Connect();
        ExpectSetPhy();
        ExecuteAllActions();

        // The completion is still outstanding, so the procedure's storage is still held.
        Disconnect();
        Connect();
        ExecuteAllActions();
    }

    TEST(LinkConfiguringGapCentralConfigurationTest, applies_the_phy_and_data_length_it_is_configured_with)
    {
        infra::EventDispatcherFixture eventDispatcher;
        testing::StrictMock<GapCentralMock> gap;
        const LinkConfiguringGapCentral::Configuration configuration{ GapPhy::le1M, GapPhy::le1M, GapDataLength{ 100, 1000 } };
        LinkConfiguringGapCentral linkConfiguring{ gap, configuration };

        infra::Function<void(GapCentral::Result)> onStepDone;

        EXPECT_CALL(gap, SetPhy(GapPhy::le1M, GapPhy::le1M, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<2>(&onStepDone), testing::Return(GapRequestStatus::accepted)));

        gap.ChangeState(GapCentralState::connected);
        eventDispatcher.ExecuteAllActions();

        EXPECT_CALL(gap, SetDataLength(configuration.dataLength, testing::_)).WillOnce(testing::Return(GapRequestStatus::accepted));

        infra::PostAssign(onStepDone, nullptr)(GapCentral::Result::success);
    }

    TEST(LinkConfiguringGapCentralConfigurationTest, asks_for_the_longest_payload_on_the_slowest_phy_by_default)
    {
        EXPECT_EQ(GapPhy::le2M, LinkConfiguringGapCentral::defaultConfiguration.txPhy);
        EXPECT_EQ(GapPhy::le2M, LinkConfiguringGapCentral::defaultConfiguration.rxPhy);
        EXPECT_EQ(GapDataLength::initialMaxTxOctets, LinkConfiguringGapCentral::defaultConfiguration.dataLength.maxTxOctets);
        EXPECT_EQ(GapDataLength::InitialMaxTxTime(GapPhy::le1M), LinkConfiguringGapCentral::defaultConfiguration.dataLength.maxTxTime);
    }
}
