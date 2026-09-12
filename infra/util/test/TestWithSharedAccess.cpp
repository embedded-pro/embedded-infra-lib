#include "infra/util/WithSharedAccess.hpp"
#include "gmock/gmock.h"

namespace
{
    class Value
        : public infra::EnableSharedFromThis<Value>
    {
    public:
        explicit Value(int v)
            : v(v)
        {}

        int Get() const
        {
            return v;
        }

    private:
        int v;
    };
}

TEST(WithSharedAccessTest, ArrowOperatorDelegatesToContainedObject)
{
    infra::WithSharedAccess<Value> wrapper{ 42 };
    EXPECT_EQ(wrapper->Get(), 42);
}

TEST(WithSharedAccessTest, DereferenceOperatorDelegatesToContainedObject)
{
    infra::WithSharedAccess<Value> wrapper{ 7 };
    EXPECT_EQ((*wrapper).Get(), 7);
}

TEST(WithSharedAccessTest, WeakPtrIsValidWhileWrapperIsAlive)
{
    infra::WithSharedAccess<Value> wrapper{ 1 };
    infra::WeakPtr<Value> weak = wrapper->WeakFromThis();
    EXPECT_NE(weak.lock(), nullptr);
}

TEST(WithSharedAccessTest, WeakPtrFromThisGivesAccessToObject)
{
    infra::WithSharedAccess<Value> wrapper{ 99 };
    infra::WeakPtr<Value> weak = wrapper->WeakFromThis();

    auto shared = weak.lock();
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->Get(), 99);
}

TEST(WithSharedAccessTest, MultipleWeakPtrsFromSameWrapperAreAllValid)
{
    infra::WithSharedAccess<Value> wrapper{ 5 };
    auto w1 = wrapper->WeakFromThis();
    auto w2 = wrapper->WeakFromThis();

    EXPECT_NE(w1.lock(), nullptr);
    EXPECT_NE(w2.lock(), nullptr);
}
