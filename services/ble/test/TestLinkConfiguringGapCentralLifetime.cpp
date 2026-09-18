#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/PostAssign.hpp"
#include "services/ble/LinkConfiguringGapCentral.hpp"
#include "services/ble/test_doubles/GapCentralMock.hpp"
#include "gmock/gmock.h"
#include <optional>

namespace services
{
    namespace
    {
        class LinkConfiguringGapCentralLifetimeTest
            : public testing::Test
            , public infra::EventDispatcherFixture
        {
        public:
            testing::StrictMock<GapCentralMock> gap;
            std::optional<LinkConfiguringGapCentral> linkConfiguring{ std::in_place, gap };
            infra::Function<void(GapCentral::Result)> onStepDone;

            void ConnectAndStartTheFirstStep()
            {
                EXPECT_CALL(gap, SetPhy(testing::_, testing::_, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<2>(&onStepDone), testing::Return(GapRequestStatus::accepted)));

                gap.ChangeState(GapCentralState::connected);
                ExecuteAllActions();
            }

            void CompleteStep(GapCentral::Result result = GapCentral::Result::success)
            {
                infra::PostAssign(onStepDone, nullptr)(result);
            }
        };
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_while_idle)
    {
        linkConfiguring.reset();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_once_the_procedure_has_finished)
    {
        ConnectAndStartTheFirstStep();

        EXPECT_CALL(gap, SetDataLength(testing::_, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<1>(&onStepDone), testing::Return(GapRequestStatus::accepted)));
        CompleteStep();
        CompleteStep();

        linkConfiguring.reset();
    }

    // The sequence GapCentralSt::HandleHciDisconnectEvent produces: the connection handle is
    // invalidated, the pending procedures are completed, and only then is standby reported. The
    // step completion therefore runs while the decorator still believes it is connected, and the
    // follow-up step is refused because the link is already gone.
    TEST_F(LinkConfiguringGapCentralLifetimeTest, survives_the_disconnect_sequence_a_port_reports)
    {
        ConnectAndStartTheFirstStep();

        EXPECT_CALL(gap, SetDataLength(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));
        CompleteStep(GapCentral::Result::controllerError);

        gap.ChangeState(GapCentralState::standby);

        linkConfiguring.reset();
    }

    // A disconnect the controller reports as failed leaves the handle valid, so the follow-up step
    // is accepted and its completion is one the controller will never send.
    TEST_F(LinkConfiguringGapCentralLifetimeTest, a_step_the_controller_never_reports_holds_the_procedure)
    {
        ConnectAndStartTheFirstStep();

        EXPECT_CALL(gap, SetDataLength(testing::_, testing::_)).WillOnce(testing::DoAll(testing::SaveArg<1>(&onStepDone), testing::Return(GapRequestStatus::accepted)));
        CompleteStep(GapCentral::Result::controllerError);

        gap.ChangeState(GapCentralState::standby);

        // Releasing the completion is what a port does from its disconnect handling; without it the
        // procedure stays held and this decorator cannot be destroyed.
        onStepDone = nullptr;

        linkConfiguring.reset();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_when_the_controller_refused_every_step)
    {
        EXPECT_CALL(gap, SetPhy(testing::_, testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));
        EXPECT_CALL(gap, SetDataLength(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        gap.ChangeState(GapCentralState::connected);
        ExecuteAllActions();

        linkConfiguring.reset();
    }
}
