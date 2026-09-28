#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

class PinPoolTest
    : public testing::Test
{
public:
    services::hil::FakePinFactory factory;
    std::array<services::hil::PinId, 1> reserved{ { { 0, 1 } } };
    services::hil::PinPool::WithCapacity<2> pool{ factory, infra::MakeRange(std::as_const(reserved)) };
    hal::GpioPin* pin = nullptr;
};

TEST_F(PinPoolTest, claim_constructs_pin_with_options)
{
    services::hil::PinOptions options{ services::hil::Pull::up, false, 2 };

    EXPECT_EQ(services::hil::Status::done, pool.Claim({ 1, 2 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin, options));
    EXPECT_EQ(&factory.pins[0], pin);
    EXPECT_EQ((services::hil::PinId{ 1, 2 }), factory.constructed[0]);
    EXPECT_EQ(services::hil::Pull::up, factory.options[0].pull);
    EXPECT_EQ(2, factory.options[0].drive);
}

TEST_F(PinPoolTest, claim_rejects_invalid_and_reserved_pins)
{
    EXPECT_EQ(services::hil::Status::pin, pool.Claim({ 6, 0 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
    EXPECT_EQ(services::hil::Status::pin, pool.Claim({ 0, 8 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
    EXPECT_EQ(services::hil::Status::busy, pool.Claim({ 0, 1 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
}

TEST_F(PinPoolTest, exclusive_pin_cannot_be_claimed_twice)
{
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 2 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));

    EXPECT_EQ(services::hil::Status::busy, pool.Claim({ 1, 2 }, services::hil::owner::uart, services::hil::PinPool::Use::exclusive, pin));
    EXPECT_EQ(services::hil::Status::busy, pool.ClaimFunction({ 1, 2 }, services::hil::owner::uart, 3, 0, pin));
}

TEST_F(PinPoolTest, analog_pin_is_shared)
{
    hal::GpioPin* other = nullptr;
    ASSERT_EQ(services::hil::Status::done, pool.ClaimAnalog({ 4, 0 }, services::hil::owner::adc, pin));

    EXPECT_EQ(services::hil::Status::done, pool.ClaimAnalog({ 4, 0 }, services::hil::owner::adc + 1, other));
    EXPECT_EQ(pin, other);

    pool.Release(services::hil::owner::adc);
    EXPECT_TRUE(factory.constructed[0].has_value());

    pool.Release(services::hil::owner::adc + 1);
    EXPECT_FALSE(factory.constructed[0].has_value());
}

TEST_F(PinPoolTest, analog_claim_requires_analog_support)
{
    EXPECT_EQ(services::hil::Status::pin, pool.ClaimAnalog({ 1, 0 }, services::hil::owner::adc, pin));
}

TEST_F(PinPoolTest, function_claim_requires_function_support)
{
    EXPECT_EQ(services::hil::Status::pin, pool.ClaimFunction({ 5, 7 }, services::hil::owner::uart, 3, 0, pin));
    EXPECT_EQ(services::hil::Status::done, pool.ClaimFunction({ 5, 6 }, services::hil::owner::uart, 3, 0, pin));
}

TEST_F(PinPoolTest, absent_optional_pin_claims_nothing)
{
    pin = &factory.pins[1];

    EXPECT_EQ(services::hil::Status::done, pool.ClaimFunction(std::optional<services::hil::PinId>(), services::hil::owner::uart, 3, 0, pin));
    EXPECT_EQ(nullptr, pin);
    EXPECT_FALSE(factory.constructed[0].has_value());
}

TEST_F(PinPoolTest, full_pool_reports_busy)
{
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 0 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 1 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));

    EXPECT_EQ(services::hil::Status::busy, pool.Claim({ 1, 2 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
}

TEST_F(PinPoolTest, release_by_pin_keeps_other_pins_of_owner)
{
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 0 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 1 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));

    pool.Release({ 1, 0 }, services::hil::owner::gpio);

    EXPECT_FALSE(factory.constructed[0].has_value());
    EXPECT_TRUE(factory.constructed[1].has_value());
    EXPECT_EQ(services::hil::Status::done, pool.Claim({ 1, 2 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));
}

TEST_F(PinPoolTest, release_ignores_other_owners)
{
    ASSERT_EQ(services::hil::Status::done, pool.Claim({ 1, 0 }, services::hil::owner::gpio, services::hil::PinPool::Use::exclusive, pin));

    pool.Release(services::hil::owner::uart);
    pool.Release({ 1, 0 }, services::hil::owner::uart);

    EXPECT_TRUE(factory.constructed[0].has_value());
}

TEST_F(PinPoolTest, pin_owner_claims_and_releases_for_its_owner)
{
    services::hil::PinOwner owner{ pool, services::hil::owner::spi };
    hal::GpioPin* analog = nullptr;

    EXPECT_EQ(services::hil::Status::done, owner.ClaimFunction(services::hil::PinId{ 1, 0 }, 1, 0, pin));
    EXPECT_EQ(services::hil::Status::done, owner.ClaimAnalog({ 4, 1 }, analog));
    EXPECT_EQ(services::hil::owner::spi, owner.Id());
    EXPECT_EQ(&pool, &owner.Pool());

    owner.Release({ 1, 0 });
    EXPECT_FALSE(factory.constructed[0].has_value());

    owner.Release();
    EXPECT_FALSE(factory.constructed[1].has_value());
}
