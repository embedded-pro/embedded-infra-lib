#include "infra/util/Function.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    struct MoveOnly
    {
        explicit MoveOnly(int value)
            : value(value)
        {}

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        MoveOnly(MoveOnly&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
        }

        int value;
    };

    template<class Invoker>
    struct MidAssignmentProbe
    {
        Invoker* invoker{ nullptr };
        int invocations{ 0 };
        bool armed{ false };
    };

    template<class Invoker>
    struct InvokeOnCopy
    {
        explicit InvokeOnCopy(MidAssignmentProbe<Invoker>& probe)
            : probe(&probe)
        {}

        InvokeOnCopy(const InvokeOnCopy& other)
            : probe(other.probe)
        {
            if (probe->armed)
            {
                ++probe->invocations;
                (*probe->invoker)();
            }
        }

        InvokeOnCopy& operator=(const InvokeOnCopy&) = delete;

        MidAssignmentProbe<Invoker>* probe;
    };

    void AssignInfraFunctionWhileInvoking()
    {
        infra::Function<void()> plain = []() {};
        MidAssignmentProbe<infra::Function<void()>> probe;
        probe.invoker = &plain;
        InvokeOnCopy<infra::Function<void()>> hook{ probe };
        infra::Function<void()> replacement = [hook]() {};

        probe.armed = true;
        plain = replacement;
    }

    class HotSwapFunctionTest
        : public testing::Test
    {
    public:
        infra::HotSwapFunction<void(int)> hotSwap;
        int total{ 0 };
    };
}

TEST_F(HotSwapFunctionTest, a_function_that_was_never_set_is_skipped)
{
    hotSwap(5);

    EXPECT_EQ(total, 0);
    EXPECT_FALSE(hotSwap.IsSet());
}

TEST_F(HotSwapFunctionTest, a_set_function_is_invoked_with_the_arguments)
{
    hotSwap.Set([this](int value)
        {
            total += value;
        });

    hotSwap(5);
    hotSwap(2);

    EXPECT_EQ(total, 7);
    EXPECT_TRUE(hotSwap.IsSet());
}

TEST_F(HotSwapFunctionTest, a_replacement_takes_over_from_the_previous_function)
{
    hotSwap.Set([this](int)
        {
            total += 1;
        });
    hotSwap.Set([this](int)
        {
            total += 100;
        });

    hotSwap(0);

    EXPECT_EQ(total, 100);
}

TEST_F(HotSwapFunctionTest, a_cleared_function_is_skipped)
{
    hotSwap.Set([this](int)
        {
            ++total;
        });
    hotSwap.Clear();

    hotSwap(0);

    EXPECT_EQ(total, 0);
    EXPECT_FALSE(hotSwap.IsSet());
}

TEST_F(HotSwapFunctionTest, a_function_can_be_set_again_after_clear)
{
    hotSwap.Set([this](int)
        {
            total += 1;
        });
    hotSwap.Clear();
    hotSwap.Set([this](int)
        {
            total += 10;
        });

    hotSwap(0);

    EXPECT_EQ(total, 10);
    EXPECT_TRUE(hotSwap.IsSet());
}

TEST_F(HotSwapFunctionTest, a_function_can_clear_itself_while_it_runs)
{
    hotSwap.Set([this](int)
        {
            ++total;
            hotSwap.Clear();
            ++total;
        });

    hotSwap(0);
    hotSwap(0);

    EXPECT_EQ(total, 2);
}

TEST_F(HotSwapFunctionTest, a_function_can_replace_itself_while_it_runs)
{
    hotSwap.Set([this](int)
        {
            total += 1;
            hotSwap.Set([this](int)
                {
                    total += 10;
                });
            total += 1;
        });

    hotSwap(0);
    hotSwap(0);

    EXPECT_EQ(total, 12);
}

TEST_F(HotSwapFunctionTest, repeated_replacement_alternates_between_the_slots_without_losing_the_latest)
{
    for (int generation = 1; generation != 8; ++generation)
    {
        hotSwap.Set([this, generation](int)
            {
                total = generation;
            });
        hotSwap(0);

        EXPECT_EQ(total, generation);
    }
}

TEST_F(HotSwapFunctionTest, arguments_are_forwarded_to_the_function)
{
    infra::HotSwapFunction<void(MoveOnly, MoveOnly&&, int&)> forwarding;
    int received = 0;
    int moved = 0;

    forwarding.Set([&received, &moved](MoveOnly byValue, MoveOnly&& byReference, int& out)
        {
            received = byValue.value;
            MoveOnly taken{ std::move(byReference) };
            moved = taken.value;
            out = 3;
        });

    MoveOnly second{ 8 };
    int out = 0;
    forwarding(MoveOnly{ 7 }, std::move(second), out);

    EXPECT_EQ(received, 7);
    EXPECT_EQ(moved, 8);
    EXPECT_EQ(second.value, -1);
    EXPECT_EQ(out, 3);
}

TEST_F(HotSwapFunctionTest, extra_size_allows_larger_captures)
{
    infra::HotSwapFunction<void(), 32> large;
    int a = 0, b = 0, c = 0, d = 0;

    large.Set([&a, &b, &c, &d]()
        {
            a = b = c = d = 4;
        });
    large();

    EXPECT_EQ(a + b + c + d, 16);
}

TEST_F(HotSwapFunctionTest, an_invoker_firing_in_the_middle_of_set_sees_a_complete_old_function)
{
    infra::HotSwapFunction<void()> swapped;
    MidAssignmentProbe<infra::HotSwapFunction<void()>> probe;
    probe.invoker = &swapped;
    int observed = 0;

    swapped.Set([&observed]()
        {
            observed = 1;
        });

    InvokeOnCopy<infra::HotSwapFunction<void()>> hook{ probe };
    probe.armed = true;
    swapped.Set([hook, &observed]()
        {
            observed = 2;
        });
    probe.armed = false;

    EXPECT_GT(probe.invocations, 0);
    EXPECT_EQ(observed, 1);

    swapped();
    EXPECT_EQ(observed, 2);
}

TEST_F(HotSwapFunctionTest, the_same_invoker_in_the_middle_of_an_infra_function_assignment_aborts)
{
    EXPECT_DEATH(AssignInfraFunctionWhileInvoking(), "");
}
