#include "infra/event/EventDispatcher.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>

template<class Fixture>
class ExecutionProgressTest
    : public testing::Test
    , public Fixture
{
public:
    uint32_t CurrentSteps() const
    {
        return infra::EventDispatcher::Instance().Progress().Steps();
    }

    uint32_t stepsAtStart = 0;
    uint32_t stepsLater = 0;
};

using Fixtures = testing::Types<infra::EventDispatcherFixture, infra::EventDispatcherWithWeakPtrFixture>;
TYPED_TEST_SUITE(ExecutionProgressTest, Fixtures);

TYPED_TEST(ExecutionProgressTest, an_idle_dispatcher_is_not_executing)
{
    EXPECT_FALSE(infra::ExecutionProgress::IsExecuting(this->CurrentSteps()));
}

TYPED_TEST(ExecutionProgressTest, a_running_action_is_executing)
{
    infra::EventDispatcher::Instance().Schedule([this]()
        {
            this->stepsAtStart = this->CurrentSteps();
        });

    this->ExecuteAllActions();

    EXPECT_TRUE(infra::ExecutionProgress::IsExecuting(this->stepsAtStart));
    EXPECT_FALSE(infra::ExecutionProgress::IsExecuting(this->CurrentSteps()));
}

TYPED_TEST(ExecutionProgressTest, steps_do_not_advance_while_an_action_runs)
{
    this->stepsLater = 1;
    infra::EventDispatcher::Instance().Schedule([this]()
        {
            this->stepsAtStart = this->CurrentSteps();
            this->stepsLater = this->CurrentSteps();
        });

    this->ExecuteAllActions();

    EXPECT_EQ(this->stepsAtStart, this->stepsLater);
}

TYPED_TEST(ExecutionProgressTest, each_action_advances_the_steps_when_it_starts_and_when_it_finishes)
{
    auto before = this->CurrentSteps();
    infra::EventDispatcher::Instance().Schedule([]() {});
    infra::EventDispatcher::Instance().Schedule([]() {});

    this->ExecuteAllActions();

    EXPECT_EQ(before + 4, this->CurrentSteps());
}
