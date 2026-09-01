#include "hal/cortex_m/LowPowerStrategyCortex.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class LowPowerStrategyCortexTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    hal::cortex::LowPowerStrategyCortex strategy;
};

TEST_F(LowPowerStrategyCortexTest, request_execution_does_not_crash)
{
    strategy.RequestExecution();
}

TEST_F(LowPowerStrategyCortexTest, idle_after_request_execution_returns_immediately)
{
    strategy.RequestExecution();
    strategy.Idle(*this);
}
