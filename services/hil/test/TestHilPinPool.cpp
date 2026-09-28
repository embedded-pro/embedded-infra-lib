#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

class HilPinPoolTest
    : public testing::Test
{
public:
    services::FakePinFactory factory;
    std::array<services::HilPinId, 1> reserved{ { { 0, 1 } } };
    services::HilPinPool::WithCapacity<2> pool{ factory, infra::MakeRange(std::as_const(reserved)) };
    hal::GpioPin* pin = nullptr;
};

TEST_F(HilPinPoolTest, claim_constructs_pin_with_options)
{
    services::HilPinOptions options{ services::HilPull::up, false, 2 };

    EXPECT_EQ(services::HilStatus::done, pool.Claim({ 1, 2 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin, options));
    EXPECT_EQ(&factory.pins[0], pin);
    EXPECT_EQ((services::HilPinId{ 1, 2 }), factory.constructed[0]);
    EXPECT_EQ(services::HilPull::up, factory.options[0].pull);
    EXPECT_EQ(2, factory.options[0].drive);
}

TEST_F(HilPinPoolTest, claim_rejects_invalid_and_reserved_pins)
{
    EXPECT_EQ(services::HilStatus::pin, pool.Claim({ 6, 0 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
    EXPECT_EQ(services::HilStatus::pin, pool.Claim({ 0, 8 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
    EXPECT_EQ(services::HilStatus::busy, pool.Claim({ 0, 1 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
}

TEST_F(HilPinPoolTest, exclusive_pin_cannot_be_claimed_twice)
{
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 2 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));

    EXPECT_EQ(services::HilStatus::busy, pool.Claim({ 1, 2 }, services::HilOwners::uart, services::HilPinPool::Use::exclusive, pin));
    EXPECT_EQ(services::HilStatus::busy, pool.ClaimFunction({ 1, 2 }, services::HilOwners::uart, 3, 0, pin));
}

TEST_F(HilPinPoolTest, analog_pin_is_shared)
{
    hal::GpioPin* other = nullptr;
    ASSERT_EQ(services::HilStatus::done, pool.ClaimAnalog({ 4, 0 }, services::HilOwners::adc, pin));

    EXPECT_EQ(services::HilStatus::done, pool.ClaimAnalog({ 4, 0 }, services::HilOwners::adc + 1, other));
    EXPECT_EQ(pin, other);

    pool.Release(services::HilOwners::adc);
    EXPECT_TRUE(factory.constructed[0].has_value());

    pool.Release(services::HilOwners::adc + 1);
    EXPECT_FALSE(factory.constructed[0].has_value());
}

TEST_F(HilPinPoolTest, analog_claim_requires_analog_support)
{
    EXPECT_EQ(services::HilStatus::pin, pool.ClaimAnalog({ 1, 0 }, services::HilOwners::adc, pin));
}

TEST_F(HilPinPoolTest, function_claim_requires_function_support)
{
    EXPECT_EQ(services::HilStatus::pin, pool.ClaimFunction({ 5, 7 }, services::HilOwners::uart, 3, 0, pin));
    EXPECT_EQ(services::HilStatus::done, pool.ClaimFunction({ 5, 6 }, services::HilOwners::uart, 3, 0, pin));
}

TEST_F(HilPinPoolTest, absent_optional_pin_claims_nothing)
{
    pin = &factory.pins[1];

    EXPECT_EQ(services::HilStatus::done, pool.ClaimFunction(std::optional<services::HilPinId>(), services::HilOwners::uart, 3, 0, pin));
    EXPECT_EQ(nullptr, pin);
    EXPECT_FALSE(factory.constructed[0].has_value());
}

TEST_F(HilPinPoolTest, full_pool_reports_busy)
{
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 0 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 1 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));

    EXPECT_EQ(services::HilStatus::busy, pool.Claim({ 1, 2 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
}

TEST_F(HilPinPoolTest, release_by_pin_keeps_other_pins_of_owner)
{
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 0 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 1 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));

    pool.Release({ 1, 0 }, services::HilOwners::gpio);

    EXPECT_FALSE(factory.constructed[0].has_value());
    EXPECT_TRUE(factory.constructed[1].has_value());
    EXPECT_EQ(services::HilStatus::done, pool.Claim({ 1, 2 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));
}

TEST_F(HilPinPoolTest, release_ignores_other_owners)
{
    ASSERT_EQ(services::HilStatus::done, pool.Claim({ 1, 0 }, services::HilOwners::gpio, services::HilPinPool::Use::exclusive, pin));

    pool.Release(services::HilOwners::uart);
    pool.Release({ 1, 0 }, services::HilOwners::uart);

    EXPECT_TRUE(factory.constructed[0].has_value());
}

TEST_F(HilPinPoolTest, pin_owner_claims_and_releases_for_its_owner)
{
    services::HilPinOwner owner{ pool, services::HilOwners::spi };
    hal::GpioPin* analog = nullptr;

    EXPECT_EQ(services::HilStatus::done, owner.ClaimFunction(services::HilPinId{ 1, 0 }, 1, 0, pin));
    EXPECT_EQ(services::HilStatus::done, owner.ClaimAnalog({ 4, 1 }, analog));
    EXPECT_EQ(services::HilOwners::spi, owner.Id());
    EXPECT_EQ(&pool, &owner.Pool());

    owner.Release({ 1, 0 });
    EXPECT_FALSE(factory.constructed[0].has_value());

    owner.Release();
    EXPECT_FALSE(factory.constructed[1].has_value());
}
