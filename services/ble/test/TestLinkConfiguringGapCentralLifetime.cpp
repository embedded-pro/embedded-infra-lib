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

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_once_a_disconnect_has_completed_the_pending_procedure)
    {
        ConnectAndStartTheFirstStep();

        // What a port does when the link goes: GapCentralSt completes its pending procedures from
        // HandleHciDisconnectEvent, as GattClientConnectionSt::Released does for GATT.
        gap.ChangeState(GapCentralState::standby);
        CompleteStep(GapCentral::Result::controllerError);

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
