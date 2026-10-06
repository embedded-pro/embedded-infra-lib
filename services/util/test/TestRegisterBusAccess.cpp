#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace
{
    class RegisterBusAccessHalfWordTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<services::RegisterBusAccessHalfWordMock> mock;
        services::RegisterBusAccessHalfWord& bus{ mock };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };
}

TEST(RegisterBusAccessAliasTest, RegisterBusAccess_is_the_8_bit_variant)
{
    static_assert(std::is_same_v<services::RegisterBusAccess, services::GenericRegisterBusAccess<uint8_t>>);
    static_assert(std::is_same_v<services::RegisterBusAccessMock, services::GenericRegisterBusAccessMock<uint8_t>>);
}

TEST(RegisterBusAccessAliasTest, RegisterBusAccessHalfWord_is_the_16_bit_variant)
{
    static_assert(std::is_same_v<services::RegisterBusAccessHalfWord, services::GenericRegisterBusAccess<uint16_t>>);
    static_assert(std::is_same_v<services::RegisterBusAccessHalfWordMock, services::GenericRegisterBusAccessMock<uint16_t>>);
}

TEST(RegisterBusAccessAliasTest, the_variants_cannot_be_mixed_up)
{
    static_assert(!std::is_convertible_v<services::RegisterBusAccessHalfWordMock&, services::RegisterBusAccess&>);
    static_assert(!std::is_convertible_v<services::RegisterBusAccessMock&, services::RegisterBusAccessHalfWord&>);
}

TEST_F(RegisterBusAccessHalfWordTest, a_write_reaches_the_implementation_with_the_whole_address)
{
    EXPECT_CALL(mock, WriteRegisterMock(0x0102, std::vector<uint8_t>{ 0x03, 0x04 }));
    std::array<uint8_t, 2> data{ 0x03, 0x04 };

    bus.WriteRegister(0x0102, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(done, callback());
    ExecuteAllActions();
}

TEST_F(RegisterBusAccessHalfWordTest, a_read_reaches_the_implementation_with_the_whole_address)
{
    EXPECT_CALL(mock, ReadRegisterMock(0x0102, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x03, 0x04 }));
    std::array<uint8_t, 2> data{};

    bus.ReadRegister(0x0102, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(done, callback());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 2>{ 0x03, 0x04 }), data);
}
