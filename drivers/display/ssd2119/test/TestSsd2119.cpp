#include "drivers/display/ssd2119/Ssd2119.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
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
    constexpr hal::DisplaySize panelSize{ 320, 240 };

    constexpr std::array<drivers::Ssd2119::Step, 2> beforeEntryMode{ { { 0x10, 0x0001, 0 }, { 0x00, 0x0001, 30 } } };
    constexpr std::array<drivers::Ssd2119::Step, 2> afterEntryMode{ { { 0x07, 0x0033, 0 }, { 0x0c, 0x0005, 0 } } };

    std::vector<uint8_t> Word(uint16_t value)
    {
        return { static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) };
    }

    class Ssd2119Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static drivers::Ssd2119::Panel MakePanel(bool mirrorX, bool mirrorY)
        {
            return { panelSize, mirrorX, mirrorY, beforeEntryMode, afterEntryMode };
        }

        void ExpectRegister(uint8_t index, uint16_t value)
        {
            EXPECT_CALL(bus, WriteRegisterMock(index, Word(value)));
        }

        void ExpectInitialization(uint16_t entryMode)
        {
            testing::InSequence sequence;
            ExpectRegister(0x10, 0x0001);
            ExpectRegister(0x00, 0x0001);
            ExpectRegister(0x11, entryMode);
            ExpectRegister(0x07, 0x0033);
            ExpectRegister(0x0c, 0x0005);
        }

        void Create(bool mirrorX, bool mirrorY, hal::PixelFormat format)
        {
            panel = MakePanel(mirrorX, mirrorY);
            display.emplace(bus, reset, panel, format, [this]()
                {
                    initialized.callback();
                });
        }

        void CreateAndInitialize(bool mirrorX = false, bool mirrorY = false, hal::PixelFormat format = hal::PixelFormat::rgb565Swapped, uint16_t entryMode = 0x6830)
        {
            ExpectInitialization(entryMode);
            EXPECT_CALL(initialized, callback());
            Create(mirrorX, mirrorY, format);

            ForwardTime(std::chrono::milliseconds(60));
        }

        void ExpectWindow(uint16_t horizontalStart, uint16_t horizontalEnd, uint16_t vertical, uint16_t counterX, uint16_t counterY)
        {
            ExpectRegister(0x45, horizontalStart);
            ExpectRegister(0x46, horizontalEnd);
            ExpectRegister(0x44, vertical);
            ExpectRegister(0x4e, counterX);
            ExpectRegister(0x4f, counterY);
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinSpy reset;
        testing::StrictMock<infra::MockCallback<void()>> initialized;
        drivers::Ssd2119::Panel panel{ MakePanel(false, false) };
        std::optional<drivers::Ssd2119> display;
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
    };
}

TEST_F(Ssd2119Test, reset_is_held_low_for_the_reset_pulse_and_then_released)
{
    Create(false, false, hal::PixelFormat::rgb565Swapped);

    EXPECT_FALSE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(9));
    EXPECT_FALSE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(reset.GetStubState());
}

TEST_F(Ssd2119Test, no_register_is_written_while_the_controller_recovers_from_reset)
{
    Create(false, false, hal::PixelFormat::rgb565Swapped);

    ForwardTime(std::chrono::milliseconds(29));
}

TEST_F(Ssd2119Test, initialization_steps_are_written_in_order_with_the_entry_mode_in_between)
{
    ExpectInitialization(0x6830);
    EXPECT_CALL(initialized, callback());
    Create(false, false, hal::PixelFormat::rgb565Swapped);

    ForwardTime(std::chrono::milliseconds(60));
}

TEST_F(Ssd2119Test, a_step_delay_postpones_the_next_register_write)
{
    {
        testing::InSequence sequence;
        ExpectRegister(0x10, 0x0001);
        ExpectRegister(0x00, 0x0001);
    }
    Create(false, false, hal::PixelFormat::rgb565Swapped);

    ForwardTime(std::chrono::milliseconds(59));

    testing::Mock::VerifyAndClearExpectations(&bus);
}

TEST_F(Ssd2119Test, initialization_is_reported_once_after_the_last_step)
{
    ExpectInitialization(0x6830);
    Create(false, false, hal::PixelFormat::rgb565Swapped);
    ForwardTime(std::chrono::milliseconds(59));

    EXPECT_CALL(initialized, callback());
    ForwardTime(std::chrono::milliseconds(1));
    ForwardTime(std::chrono::milliseconds(100));
}

TEST_F(Ssd2119Test, entry_mode_selects_the_65k_colour_mode_and_decrementing_counters_for_a_mirrored_panel)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb565Swapped, 0x6800);
}

TEST_F(Ssd2119Test, entry_mode_lets_the_horizontal_counter_decrement_when_only_x_is_mirrored)
{
    CreateAndInitialize(true, false, hal::PixelFormat::rgb565Swapped, 0x6820);
}

TEST_F(Ssd2119Test, entry_mode_lets_the_vertical_counter_decrement_when_only_y_is_mirrored)
{
    CreateAndInitialize(false, true, hal::PixelFormat::rgb565Swapped, 0x6810);
}

TEST_F(Ssd2119Test, entry_mode_selects_the_262k_colour_mode_for_rgb888)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb888, 0x4800);
}

TEST_F(Ssd2119Test, native_rgb565_uses_the_65k_colour_mode)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb565, 0x6800);
}

TEST_F(Ssd2119Test, reset_pin_is_optional)
{
    ExpectInitialization(0x6830);
    EXPECT_CALL(initialized, callback());
    display.emplace(bus, hal::dummyPin, panel, hal::PixelFormat::rgb565Swapped, [this]()
        {
            initialized.callback();
        });

    ForwardTime(std::chrono::milliseconds(60));
}

TEST_F(Ssd2119Test, size_and_format_are_reported)
{
    Create(false, false, hal::PixelFormat::rgb888);

    EXPECT_EQ(panelSize, display->Size());
    EXPECT_EQ(hal::PixelFormat::rgb888, display->Format());
}

TEST_F(Ssd2119Test, a_grey_format_is_not_supported)
{
    EXPECT_DEATH(Create(false, false, hal::PixelFormat::grey8), "");
}

TEST_F(Ssd2119Test, writing_before_initialization_has_completed_asserts)
{
    Create(false, false, hal::PixelFormat::rgb565Swapped);
    std::array<uint8_t, 4> pixels{};

    EXPECT_DEATH(display->Write({ 0, 0, 2, 1 }, pixels, onDone), "");
}

TEST_F(Ssd2119Test, packed_write_sets_the_window_and_the_counter_before_streaming_the_pixels)
{
    CreateAndInitialize();
    std::array<uint8_t, 12> pixels{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    {
        testing::InSequence sequence;
        ExpectWindow(10, 12, 0x1514, 10, 20);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->Write({ 10, 20, 3, 2 }, pixels, onDone);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(Ssd2119Test, a_mirrored_panel_maps_the_window_to_the_opposite_corner_and_starts_the_counter_at_its_far_end)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb565Swapped, 0x6800);
    std::array<uint8_t, 12> pixels{};
    {
        testing::InSequence sequence;
        ExpectWindow(307, 309, 0xdbda, 309, 219);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->Write({ 10, 20, 3, 2 }, pixels, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, the_first_logical_pixel_of_a_mirrored_panel_is_the_last_gddram_position)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb565Swapped, 0x6800);
    std::array<uint8_t, 2> pixels{ 0xf8, 0x00 };
    {
        testing::InSequence sequence;
        ExpectWindow(319, 319, 0xefef, 319, 239);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0xf8, 0x00 }));
    }

    display->Write({ 0, 0, 1, 1 }, pixels, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, mirroring_only_x_leaves_the_vertical_axis_untouched)
{
    CreateAndInitialize(true, false, hal::PixelFormat::rgb565Swapped, 0x6820);
    std::array<uint8_t, 4> pixels{};
    {
        testing::InSequence sequence;
        ExpectWindow(316, 317, 0x0505, 317, 5);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->Write({ 2, 5, 2, 1 }, pixels, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, a_strided_write_resets_the_counter_for_every_row)
{
    CreateAndInitialize();
    std::array<uint8_t, 12> pixels{ 1, 2, 3, 4, 0xee, 0xee, 5, 6, 7, 8, 0xee, 0xee };
    {
        testing::InSequence sequence;
        ExpectWindow(4, 5, 0x0807, 4, 7);
        ExpectRegister(0x4e, 4);
        ExpectRegister(0x4f, 7);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, (std::vector<uint8_t>{ 1, 2, 3, 4 })));
        ExpectRegister(0x4e, 4);
        ExpectRegister(0x4f, 8);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, (std::vector<uint8_t>{ 5, 6, 7, 8 })));
    }

    display->WriteWithStride({ 4, 7, 2, 2 }, pixels, 6, onDone);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(Ssd2119Test, a_strided_write_on_a_mirrored_panel_steps_the_row_address_downwards)
{
    CreateAndInitialize(true, true, hal::PixelFormat::rgb565Swapped, 0x6800);
    std::array<uint8_t, 8> pixels{ 1, 2, 0xee, 0xee, 3, 4, 0xee, 0xee };
    {
        testing::InSequence sequence;
        ExpectWindow(319, 319, 0xeeed, 319, 238);
        ExpectRegister(0x4e, 319);
        ExpectRegister(0x4f, 238);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, (std::vector<uint8_t>{ 1, 2 })));
        ExpectRegister(0x4e, 319);
        ExpectRegister(0x4f, 237);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, (std::vector<uint8_t>{ 3, 4 })));
    }

    display->WriteWithStride({ 0, 1, 1, 2 }, pixels, 4, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, a_stride_that_equals_the_row_length_streams_all_rows_in_one_write)
{
    CreateAndInitialize();
    std::array<uint8_t, 8> pixels{ 1, 2, 3, 4, 5, 6, 7, 8 };
    {
        testing::InSequence sequence;
        ExpectWindow(0, 1, 0x0100, 0, 0);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->WriteWithStride({ 0, 0, 2, 2 }, pixels, 4, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, a_single_row_is_streamed_in_one_write_whatever_the_stride)
{
    CreateAndInitialize();
    std::array<uint8_t, 4> pixels{ 1, 2, 3, 4 };
    {
        testing::InSequence sequence;
        ExpectWindow(0, 1, 0x0000, 0, 0);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->WriteWithStride({ 0, 0, 2, 1 }, pixels, 100, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, rgb888_rows_are_three_bytes_per_pixel)
{
    CreateAndInitialize(false, false, hal::PixelFormat::rgb888, 0x4830);
    std::array<uint8_t, 6> pixels{ 1, 2, 3, 4, 5, 6 };
    {
        testing::InSequence sequence;
        ExpectWindow(0, 1, 0x0000, 0, 0);
        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>(pixels.begin(), pixels.end())));
    }

    display->Write({ 0, 0, 2, 1 }, pixels, onDone);
    ExecuteAllActions();
}

TEST_F(Ssd2119Test, write_does_not_complete_before_the_bus_is_done)
{
    CreateAndInitialize();
    bus.completeAutomatically = false;
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(testing::AtLeast(1));

    display->Write({ 0, 0, 2, 1 }, pixels, onDone);

    EXPECT_EQ(0, completions);
}

TEST_F(Ssd2119Test, write_completes_after_the_pixels_were_accepted_by_the_bus)
{
    CreateAndInitialize();
    bus.completeAutomatically = false;
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    display->Write({ 0, 0, 2, 1 }, pixels, onDone);

    for (int step = 0; step != 6; ++step)
    {
        ASSERT_TRUE(bus.CompletionPending());
        EXPECT_EQ(0, completions);
        bus.CompletePending();
    }

    EXPECT_EQ(1, completions);
}

TEST_F(Ssd2119Test, a_new_write_can_be_started_from_the_completion_callback)
{
    CreateAndInitialize();
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(12);

    display->Write({ 0, 0, 2, 1 }, pixels, [&]()
        {
            display->Write({ 0, 1, 2, 1 }, pixels, onDone);
        });
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(Ssd2119Test, write_while_another_is_in_flight_asserts)
{
    CreateAndInitialize();
    bus.completeAutomatically = false;
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    display->Write({ 0, 0, 2, 1 }, pixels, onDone);

    EXPECT_DEATH(display->Write({ 0, 1, 2, 1 }, pixels, onDone), "");
}

TEST_F(Ssd2119Test, write_outside_the_panel_asserts)
{
    CreateAndInitialize();
    std::array<uint8_t, 4> pixels{};

    EXPECT_DEATH(display->Write({ 319, 0, 2, 1 }, pixels, onDone), "");
}

TEST_F(Ssd2119Test, write_with_too_few_pixels_asserts)
{
    CreateAndInitialize();
    std::array<uint8_t, 2> pixels{};

    EXPECT_DEATH(display->Write({ 0, 0, 2, 1 }, pixels, onDone), "");
}

TEST_F(Ssd2119Test, an_empty_area_completes_without_touching_the_bus)
{
    CreateAndInitialize();

    display->Write({ 5, 5, 0, 0 }, infra::ConstByteRange(), onDone);
    EXPECT_EQ(0, completions);
    ForwardTime(std::chrono::milliseconds(0));

    EXPECT_EQ(1, completions);
}
