#include "drivers/display/ili9341/Ili9341.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    constexpr std::array<uint8_t, 0> noParameters{};
    constexpr std::array<uint8_t, 2> powerControlParameters{ 0x12, 0x34 };
    constexpr std::array<uint8_t, 1> pixelFormatParameters{ 0x55 };

    constexpr std::array<drivers::Ili9341::Command, 4> commands{ {
        { 0xcf, powerControlParameters, 0 },
        { 0x3a, pixelFormatParameters, 0 },
        { 0x11, noParameters, 120 },
        { 0x29, noParameters, 0 },
    } };

    constexpr std::array<drivers::Ili9341::Command, 2> commandsWithFinalDelay{ {
        { 0x11, noParameters, 0 },
        { 0x29, noParameters, 50 },
    } };

    class Ili9341Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Create(const drivers::Ili9341::Panel& newPanel)
        {
            panel = newPanel;
            display.emplace(bus, panel, [this]()
                {
                    initialized.callback();
                });
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        testing::StrictMock<infra::MockCallback<void()>> initialized;
        drivers::Ili9341::Panel panel{ commands };
        std::optional<drivers::Ili9341> display;
    };
}

TEST_F(Ili9341Test, the_commands_are_written_in_order_with_their_parameters)
{
    {
        testing::InSequence sequence;
        EXPECT_CALL(bus, WriteRegisterMock(0xcf, (std::vector<uint8_t>{ 0x12, 0x34 })));
        EXPECT_CALL(bus, WriteRegisterMock(0x3a, (std::vector<uint8_t>{ 0x55 })));
        EXPECT_CALL(bus, WriteRegisterMock(0x11, std::vector<uint8_t>{}));
        EXPECT_CALL(bus, WriteRegisterMock(0x29, std::vector<uint8_t>{}));
        EXPECT_CALL(initialized, callback());
    }

    Create({ commands });
    ForwardTime(std::chrono::milliseconds(120));
}

TEST_F(Ili9341Test, the_first_command_is_written_by_construction_and_the_next_waits_for_the_bus)
{
    bus.completeAutomatically = false;
    EXPECT_CALL(bus, WriteRegisterMock(0xcf, (std::vector<uint8_t>{ 0x12, 0x34 })));

    Create({ commands });
    ForwardTime(std::chrono::seconds(1));
    testing::Mock::VerifyAndClearExpectations(&bus);

    EXPECT_TRUE(bus.CompletionPending());
    EXPECT_CALL(bus, WriteRegisterMock(0x3a, (std::vector<uint8_t>{ 0x55 })));
    bus.CompletePending();
}

TEST_F(Ili9341Test, a_delay_postpones_the_next_command)
{
    {
        testing::InSequence sequence;
        EXPECT_CALL(bus, WriteRegisterMock(0xcf, (std::vector<uint8_t>{ 0x12, 0x34 })));
        EXPECT_CALL(bus, WriteRegisterMock(0x3a, (std::vector<uint8_t>{ 0x55 })));
        EXPECT_CALL(bus, WriteRegisterMock(0x11, std::vector<uint8_t>{}));
    }
    Create({ commands });

    ForwardTime(std::chrono::milliseconds(119));
    testing::Mock::VerifyAndClearExpectations(&bus);

    EXPECT_CALL(bus, WriteRegisterMock(0x29, std::vector<uint8_t>{}));
    EXPECT_CALL(initialized, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(Ili9341Test, the_delay_of_the_last_command_is_waited_for_before_reporting_initialization)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x11, std::vector<uint8_t>{}));
    EXPECT_CALL(bus, WriteRegisterMock(0x29, std::vector<uint8_t>{}));
    Create({ commandsWithFinalDelay });

    ForwardTime(std::chrono::milliseconds(49));

    EXPECT_CALL(initialized, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(Ili9341Test, initialization_is_reported_only_once)
{
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(2);
    EXPECT_CALL(initialized, callback());
    Create({ commandsWithFinalDelay });

    ForwardTime(std::chrono::seconds(10));
}

TEST_F(Ili9341Test, a_panel_without_commands_asserts)
{
    const drivers::Ili9341::Panel empty{};

    EXPECT_DEATH(display.emplace(bus, empty, [] {}), "");
}
