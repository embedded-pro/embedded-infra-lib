#include "infra/util/VariantDetail.hpp"
#include <variant>
#include "gtest/gtest.h"
#include <cstdint>

struct MoveableStruct
{
    MoveableStruct(int x)
        : x(x)
    {}

    MoveableStruct(const MoveableStruct& other) = delete;
    MoveableStruct& operator=(const MoveableStruct& other) = delete;

    MoveableStruct(MoveableStruct&& other)
        : x(other.x)
    {
        other.x = 0;
    }

    MoveableStruct& operator=(MoveableStruct&& other)
    {
        x = other.x;
        other.x = 0;

        return *this;
    }

    int x;
};

TEST(VariantTest, TestEmptyConstruction)
{
    std::variant<bool> v;
}

TEST(VariantTest, TestConstructionWithBool)
{
    bool b;
    std::variant<bool> v(b);
}

TEST(VariantTest, TestConstructionWithVariant)
{
    std::variant<bool, int> i(5);
    std::variant<bool, int> v(i);
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestConstructionWithNarrowVariant)
{
    std::variant<int> i(5);
    std::variant<bool, int> v(i);
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestConstructionAtIndex)
{
    std::variant<uint8_t, uint16_t> v(infra::atIndex, 1, 3);
    EXPECT_EQ(1, v.index());
    EXPECT_EQ(3, std::get<uint16_t>(v));
}

TEST(VariantTest, TestGetBool)
{
    bool b = true;
    std::variant<bool> v(b);
    EXPECT_TRUE(std::get<bool>(v));
}

TEST(VariantTest, TestAssignment)
{
    std::variant<bool, int> v(true);
    v = 5;
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestInPlaceConstruction)
{
    struct MyStruct
    {
        MyStruct(int aX, int aY)
            : x(aX)
            , y(aY){};

        int x;
        int y;
    };

    std::variant<MyStruct> v(std::in_place_type_t<MyStruct>(), 2, 3);
    EXPECT_EQ(0, v.index());
    EXPECT_EQ(2, std::get<MyStruct>(v).x);
    EXPECT_EQ(3, std::get<MyStruct>(v).y);
}

TEST(VariantTest, TestEmplace)
{
    struct MyStruct
    {
        MyStruct(int aX, int aY)
            : x(aX)
            , y(aY){};

        bool operator==(const MyStruct& other) const
        {
            return x == other.x && y == other.y;
        }

        int x;
        int y;
    };

    std::variant<bool, MyStruct> v(true);
    EXPECT_EQ(MyStruct(2, 3), v.emplace<MyStruct>(2, 3));
    EXPECT_EQ(1, v.index());
    EXPECT_EQ(2, std::get<MyStruct>(v).x);
    EXPECT_EQ(3, std::get<MyStruct>(v).y);
}

TEST(VariantTest, TestAssignmentFromVariant)
{
    std::variant<bool, int> v(true);
    v = std::variant<bool, int>(5);
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestSelfAssignment)
{
    std::variant<bool, int> v(true);
    v = v;
    EXPECT_EQ(true, std::get<bool>(v));
}

TEST(VariantTest, TestAssignmentFromNarrowVariant)
{
    std::variant<bool, int> v(true);
    v = std::variant<int>(5);
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestMoveConstruct)
{
    std::variant<MoveableStruct> v(std::in_place_type_t<MoveableStruct>(), 2);
    auto v2(std::move(v));
    EXPECT_EQ(0, v2.index());
    EXPECT_EQ(0, std::get<MoveableStruct>(v).x);
    EXPECT_EQ(2, std::get<MoveableStruct>(v2).x);
}

TEST(VariantTest, TestMoveAssign)
{
    std::variant<MoveableStruct> v(std::in_place_type_t<MoveableStruct>(), 2);
    std::variant<MoveableStruct> v2(std::in_place_type_t<MoveableStruct>(), 3);
    v2 = std::move(v);
    EXPECT_EQ(0, v2.index());
    EXPECT_EQ(0, std::get<MoveableStruct>(v).x);
    EXPECT_EQ(2, std::get<MoveableStruct>(v2).x);
}

TEST(VariantTest, TestVariantWithTwoTypes)
{
    int i = 5;
    std::variant<bool, int> v(i);
    EXPECT_EQ(5, std::get<int>(v));
}

TEST(VariantTest, TestVisitor)
{
    struct Visitor
        : infra::StaticVisitor<void>
    {
        Visitor()
            : passed(0)
        {}

        void operator()(bool b)
        {}

        void operator()(int i)
        {
            ++passed;
        }

        int passed;
    };

    int i = 5;
    std::variant<bool, int> variant(i);
    Visitor visitor;
    std::visit(visitor, variant);
    EXPECT_EQ(1, visitor.passed);
}

TEST(VariantTest, TestReturningVisitor)
{
    struct Visitor
        : infra::StaticVisitor<bool>
    {
        bool operator()(bool b)
        {
            return false;
        }

        bool operator()(int i)
        {
            return true;
        }
    };

    int i = 5;
    std::variant<bool, int> variant(i);
    Visitor visitor;
    EXPECT_EQ(true, std::visit(visitor, variant));
}

TEST(VariantTest, TestEqual)
{
    int i = 1;
    int j = 2;
    bool k = true;
    const std::variant<bool, int> v1(i);
    const std::variant<bool, int> v2(i);
    const std::variant<bool, int> v3(j);
    const std::variant<bool, int> v4(k);

    EXPECT_EQ(v1, v2);
    EXPECT_NE(v1, v3);
    EXPECT_NE(v1, v4);
}

TEST(VariantTest, TestLessThan)
{
    int i = 1;
    int j = 2;
    bool k = true;
    const std::variant<bool, int> v1(i);
    const std::variant<bool, int> v2(i);
    const std::variant<bool, int> v3(j);
    const std::variant<bool, int> v4(k);

    EXPECT_GE(v1, v2);
    EXPECT_LE(v1, v2);
    EXPECT_LT(v1, v3);
    EXPECT_GT(v1, v4);
}

TEST(VariantTest, TestEqualValue)
{
    int i = 1;
    int j = 2;
    bool k = true;
    const std::variant<bool, int> v1(i);

    EXPECT_EQ(v1, i);
    EXPECT_NE(v1, j);
    EXPECT_NE(v1, k);
}

TEST(VariantTest, TestLessThanValue)
{
    int i = 1;
    int j = 2;
    bool k = true;
    const std::variant<bool, int> v1(i);

    EXPECT_GE(v1, i);
    EXPECT_LE(v1, i);
    EXPECT_LT(v1, j);
    EXPECT_GT(v1, k);
}

struct DoubleVisitor
    : infra::StaticVisitor<int>
{
    template<class T, class U>
    int operator()(T x, U y)
    {
        return x + y;
    }
};

TEST(VariantTest, TestDoubleVisitor)
{
    std::variant<uint32_t, int> variant1(std::in_place_type_t<int>(), 5);
    std::variant<uint32_t, int> variant2(std::in_place_type_t<uint32_t>(), 1u);
    DoubleVisitor visitor;
    EXPECT_EQ(2, std::visit(visitor, variant2, variant2));
    EXPECT_EQ(6, std::visit(visitor, variant1, variant2));
    EXPECT_EQ(10, std::visit(visitor, variant1, variant1));
}

struct EmptyVisitor
    : infra::StaticVisitor<void>
{
    template<class T>
    void operator()(T)
    {}

    template<class T>
    void operator()(T, T)
    {}

    template<class T, class U>
    void operator()(T, U)
    {}
};

struct A
{};

struct B
{};

struct C
{};

struct D
{};

struct E
{};

struct F
{};

TEST(VariantTest, TestRecursiveLoopUnrolling3_1)
{
    std::variant<A, B, C> v((A()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling3_2)
{
    std::variant<A, B, C> v((B()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling3_3)
{
    std::variant<A, B, C> v((C()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling4_1)
{
    std::variant<A, B, C, D> v((A()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling4_2)
{
    std::variant<A, B, C, D> v((B()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling4_3)
{
    std::variant<A, B, C, D> v((C()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling4_4)
{
    std::variant<A, B, C, D> v((D()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling5_1)
{
    std::variant<A, B, C, D, E> v((A()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling5_2)
{
    std::variant<A, B, C, D, E> v((B()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling5_3)
{
    std::variant<A, B, C, D, E> v((C()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling5_4)
{
    std::variant<A, B, C, D, E> v((D()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrolling5_5)
{
    std::variant<A, B, C, D, E> v((E()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}

TEST(VariantTest, TestRecursiveLoopUnrollingX)
{
    std::variant<A, B, C, D, E, F> v((F()));
    EmptyVisitor visitor;
    std::visit(visitor, v);
    std::visit(visitor, v, v);
    infra::ApplySameTypeVisitor(visitor, v, v);
}
