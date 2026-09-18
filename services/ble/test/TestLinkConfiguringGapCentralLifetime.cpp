#include "infra/timer/test_helper/ClockFixture.hpp"
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
            , public infra::ClockFixture
        {
        public:
            testing::StrictMock<GapCentralMock> gap;
            std::optional<LinkConfiguringGapCentral> linkConfiguring{ std::in_place, gap };

            void Settle()
            {
                ForwardTime(std::chrono::milliseconds(1));
            }
        };
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_while_idle)
    {
        linkConfiguring.reset();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_while_a_phy_request_is_pending)
    {
        gap.ChangeState(GapCentralState::connected);

        linkConfiguring.reset();

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_while_a_data_length_request_is_pending)
    {
        gap.ChangeState(GapCentralState::connected);
        EXPECT_CALL(gap, SetPhy(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        gap.ChangePhy(GapPhy::le2M, GapPhy::le2M);

        linkConfiguring.reset();

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_while_connected)
    {
        gap.ChangeState(GapCentralState::connected);
        EXPECT_CALL(gap, SetPhy(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        linkConfiguring.reset();

        Settle();
    }

    TEST_F(LinkConfiguringGapCentralLifetimeTest, can_be_destroyed_after_a_disconnect)
    {
        gap.ChangeState(GapCentralState::connected);
        EXPECT_CALL(gap, SetPhy(testing::_, testing::_)).WillOnce(testing::Return(GapRequestStatus::accepted));
        Settle();

        gap.ChangeState(GapCentralState::standby);

        linkConfiguring.reset();

        Settle();
    }
}
