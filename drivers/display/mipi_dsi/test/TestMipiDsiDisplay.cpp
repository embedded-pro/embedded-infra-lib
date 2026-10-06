#include "drivers/display/mipi_dsi/MipiDsiDisplay.hpp"
#include "drivers/display/mipi_dsi/test/MipiDsiTestPanel.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "hal/interfaces/test_doubles/DsiHostStub.hpp"
#include "hal/interfaces/test_doubles/GpioMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

namespace
{
    using dsitest::ExpectDcs;
    using dsitest::InitializationResult;

    std::vector<uint8_t> Pixels(std::size_t count)
    {
        std::vector<uint8_t> pixels(count);

        for (std::size_t index = 0; index != count; ++index)
            pixels[index] = static_cast<uint8_t>(index + 1);

        return pixels;
    }

    std::vector<uint8_t> Slice(const std::vector<uint8_t>& data, std::size_t offset, std::size_t size)
    {
        return std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + size);
    }

    uint8_t PixelFormatByte(hal::PixelFormat format)
    {
        return format == hal::PixelFormat::rgb888 ? 0x77 : 0x55;
    }

    class MipiDsiDisplayTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Create(hal::PixelFormat format, hal::GpioPin& tearingEffect)
        {
            display.emplace(host, reset, tearingEffect, configuration, format, [this](InitializationResult result)
                {
                    initialized.callback(result);
                });
        }

        void CreateAndInitialize(hal::PixelFormat format = hal::PixelFormat::rgb565Swapped, hal::GpioPin& tearingEffect = hal::dummyPin)
        {
            {
                testing::InSequence sequence;
                dsitest::ExpectInitializationBeforeDisplayOn(host, dsitest::InitializationOptions{ PixelFormatByte(format), 0x48, &tearingEffect != &hal::dummyPin });
                ExpectDcs(host, 0x29, {});
                EXPECT_CALL(initialized, callback(InitializationResult::success));
            }

            Create(format, tearingEffect);
            ForwardTime(std::chrono::milliseconds(300));
            VerifyAll();
        }

        void VerifyAll()
        {
            testing::Mock::VerifyAndClearExpectations(&host);
            testing::Mock::VerifyAndClearExpectations(&initialized);
            testing::Mock::VerifyAndClearExpectations(&tearingPinMock);
        }

        void Write(const hal::DisplayArea& area, const std::vector<uint8_t>& pixels)
        {
            display->Write(area, infra::MakeRange(pixels), onDone);
        }

        void WriteWithStride(const hal::DisplayArea& area, const std::vector<uint8_t>& pixels, std::size_t stride)
        {
            display->WriteWithStride(area, infra::MakeRange(pixels), stride, onDone);
        }

        void ExpectColumns(uint8_t firstHigh, uint8_t firstLow, uint8_t lastHigh, uint8_t lastLow)
        {
            ExpectDcs(host, 0x2a, { firstHigh, firstLow, lastHigh, lastLow });
        }

        void ExpectPages(uint8_t firstHigh, uint8_t firstLow, uint8_t lastHigh, uint8_t lastLow)
        {
            ExpectDcs(host, 0x2b, { firstHigh, firstLow, lastHigh, lastLow });
        }

        void Sleep()
        {
            ExpectDcs(host, 0x28, {});
            ExpectDcs(host, 0x10, {});
            EXPECT_CALL(done, callback());

            display->Sleep(sleepDone);
            ForwardTime(std::chrono::milliseconds(200));
            VerifyAll();
        }

        void DestroyDisplayWithTearingPinMock(int disableCalls)
        {
            EXPECT_CALL(tearingPinMock, DisableInterrupt()).Times(disableCalls);
            EXPECT_CALL(tearingPinMock, ResetConfig());

            display.reset();
        }

        testing::StrictMock<hal::DsiHostMock> host;
        hal::GpioPinSpy reset;
        testing::StrictMock<hal::GpioPinMock> tearingPinMock;
        hal::GpioPinStub tearingPin;
        testing::StrictMock<infra::MockCallback<void(InitializationResult)>> initialized;
        testing::StrictMock<infra::MockCallback<void()>> done;
        dsitest::Panel configuration{ dsitest::MakePanel() };
        std::optional<drivers::MipiDsiDisplay> display;
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
        infra::Function<void()> sleepDone{ [this]()
            {
                done.callback();
            } };
    };

    class MipiDsiDisplayPlacementTest
        : public testing::TestWithParam<std::tuple<hal::PixelFormat, std::size_t>>
        , public infra::ClockFixture
    {
    public:
        static constexpr hal::DisplaySize smallPanelSize{ 6, 4 };

        MipiDsiDisplayPlacementTest()
        {
            configuration.size = smallPanelSize;
            host.maxParametersSize = std::get<1>(GetParam());
            dsitest::AllowAnyCommands(host);
            EXPECT_CALL(initialized, callback(InitializationResult::success));

            display.emplace(host, reset, hal::dummyPin, configuration, Format(), [this](InitializationResult result)
                {
                    initialized.callback(result);
                });
            ForwardTime(std::chrono::milliseconds(300));
        }

        hal::PixelFormat Format() const
        {
            return std::get<0>(GetParam());
        }

        std::size_t PixelBytes() const
        {
            return hal::BytesPerPixel(Format());
        }

        void ExpectPixel(uint16_t x, uint16_t y, const std::vector<uint8_t>& source, std::size_t sourceOffset)
        {
            infra::ConstByteRange pixel = host.PixelAt(x, y);

            EXPECT_EQ(Slice(source, sourceOffset, PixelBytes()), std::vector<uint8_t>(pixel.begin(), pixel.end()));
        }

        void ExpectUntouched(uint16_t x, uint16_t y)
        {
            infra::ConstByteRange pixel = host.PixelAt(x, y);

            EXPECT_EQ(std::vector<uint8_t>(PixelBytes(), 0), std::vector<uint8_t>(pixel.begin(), pixel.end()));
        }

        testing::StrictMock<hal::DsiHostStub::WithStorage<6 * 4 * 3>> host{ smallPanelSize, std::get<0>(GetParam()) };
        hal::GpioPinStub reset;
        testing::StrictMock<infra::MockCallback<void(InitializationResult)>> initialized;
        dsitest::Panel configuration{ dsitest::MakePanel() };
        std::optional<drivers::MipiDsiDisplay> display;
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
    };
}

TEST_F(MipiDsiDisplayTest, size_and_format_are_reported)
{
    CreateAndInitialize(hal::PixelFormat::rgb888);

    EXPECT_EQ(dsitest::panelSize, display->Size());
    EXPECT_EQ(hal::PixelFormat::rgb888, display->Format());
}

TEST_F(MipiDsiDisplayTest, native_rgb565_asserts)
{
    EXPECT_DEATH(Create(hal::PixelFormat::rgb565, hal::dummyPin), "");
}

TEST_F(MipiDsiDisplayTest, a_grey_format_asserts)
{
    EXPECT_DEATH(Create(hal::PixelFormat::grey8, hal::dummyPin), "");
}

TEST_F(MipiDsiDisplayTest, a_host_that_cannot_carry_the_window_parameters_asserts)
{
    host.maxParametersSize = 3;

    EXPECT_DEATH(Create(hal::PixelFormat::rgb565Swapped, hal::dummyPin), "");
}

TEST_F(MipiDsiDisplayTest, initialization_without_a_tearing_effect_pin_sends_no_tearing_effect_command)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, hal::dummyPin);
}

TEST_F(MipiDsiDisplayTest, tearing_effect_is_enabled_during_initialization_when_a_pin_is_connected)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
}

TEST_F(MipiDsiDisplayTest, writing_before_initialization_asserts)
{
    Create(hal::PixelFormat::rgb565Swapped, hal::dummyPin);
    std::vector<uint8_t> pixels = Pixels(2);

    EXPECT_DEATH(Write({ 0, 0, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, column_and_page_address_are_big_endian_with_an_inclusive_end)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(200);
    {
        testing::InSequence sequence;
        ExpectColumns(0x01, 0x2c, 0x01, 0x3f);
        ExpectPages(0x02, 0xbc, 0x02, 0xc0);
        ExpectDcs(host, 0x2c, pixels);
    }

    Write({ 300, 700, 20, 5 }, pixels);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_single_pixel_area_has_equal_start_and_end)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(2);
    {
        testing::InSequence sequence;
        ExpectColumns(0x00, 0x05, 0x00, 0x05);
        ExpectPages(0x00, 0x06, 0x00, 0x06);
        ExpectDcs(host, 0x2c, pixels);
    }

    Write({ 5, 6, 1, 1 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, a_full_screen_write_covers_the_whole_panel)
{
    CreateAndInitialize();
    host.maxParametersSize = 1000000;
    std::vector<uint8_t> pixels(480 * 800 * 2);
    {
        testing::InSequence sequence;
        ExpectColumns(0x00, 0x00, 0x01, 0xdf);
        ExpectPages(0x00, 0x00, 0x03, 0x1f);
        EXPECT_CALL(host, WriteDcsMock(0x2c, testing::_));
    }

    Write({ 0, 0, 480, 800 }, pixels);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_write_that_fits_one_chunk_is_a_single_memory_write)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 3);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, pixels);
    }

    Write({ 0, 0, 4, 3 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, a_write_larger_than_the_host_limit_continues_with_write_memory_continue)
{
    CreateAndInitialize();
    host.maxParametersSize = 10;
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 3);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 10, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 20, 4));
    }

    Write({ 0, 0, 4, 3 }, pixels);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_write_of_exactly_the_chunk_size_is_one_chunk)
{
    CreateAndInitialize();
    host.maxParametersSize = 24;
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 3);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, pixels);
    }

    Write({ 0, 0, 4, 3 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, a_write_one_pixel_over_the_chunk_size_needs_a_second_chunk)
{
    CreateAndInitialize();
    host.maxParametersSize = 22;
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 3);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 22));
        ExpectDcs(host, 0x3c, Slice(pixels, 22, 2));
    }

    Write({ 0, 0, 4, 3 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, the_chunk_size_is_rounded_down_to_whole_pixels)
{
    CreateAndInitialize();
    host.maxParametersSize = 5;
    std::vector<uint8_t> pixels = Pixels(12);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 2);
        ExpectPages(0, 0, 0, 1);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 4, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 8, 4));
    }

    Write({ 0, 0, 3, 2 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, the_chunk_size_is_rounded_down_for_rgb888)
{
    CreateAndInitialize(hal::PixelFormat::rgb888);
    host.maxParametersSize = 4;
    std::vector<uint8_t> pixels = Pixels(12);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 1);
        ExpectPages(0, 0, 0, 1);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 3));
        ExpectDcs(host, 0x3c, Slice(pixels, 3, 3));
        ExpectDcs(host, 0x3c, Slice(pixels, 6, 3));
        ExpectDcs(host, 0x3c, Slice(pixels, 9, 3));
    }

    Write({ 0, 0, 2, 2 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, rows_of_a_packed_buffer_share_chunks_across_row_boundaries)
{
    CreateAndInitialize();
    host.maxParametersSize = 10;
    std::vector<uint8_t> pixels = Pixels(12);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 1);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 10, 2));
    }

    Write({ 0, 0, 2, 3 }, pixels);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, a_strided_write_sends_each_row_separately_and_never_crosses_the_padding)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 1);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 10, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 20, 4));
    }

    WriteWithStride({ 0, 0, 2, 3 }, pixels, 10);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_strided_write_with_a_small_limit_splits_each_row_into_chunks)
{
    CreateAndInitialize();
    host.maxParametersSize = 4;
    std::vector<uint8_t> pixels = Pixels(18);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 2);
        ExpectPages(0, 0, 0, 1);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 4, 2));
        ExpectDcs(host, 0x3c, Slice(pixels, 12, 4));
        ExpectDcs(host, 0x3c, Slice(pixels, 16, 2));
    }

    WriteWithStride({ 0, 0, 3, 2 }, pixels, 12);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, a_single_row_ignores_the_stride)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(6);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 2);
        ExpectPages(0, 0, 0, 0);
        ExpectDcs(host, 0x2c, pixels);
    }

    WriteWithStride({ 0, 0, 3, 1 }, pixels, 20);
    ExecuteAllActions();
}

TEST_F(MipiDsiDisplayTest, an_empty_area_completes_without_host_traffic)
{
    CreateAndInitialize();

    Write({ 5, 5, 0, 0 }, std::vector<uint8_t>());
    EXPECT_EQ(0, completions);
    ForwardTime(std::chrono::milliseconds(0));

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, an_empty_area_does_not_wait_for_tearing_effect)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);

    Write({ 5, 5, 0, 0 }, std::vector<uint8_t>());
    ForwardTime(std::chrono::milliseconds(0));

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, write_does_not_complete_before_the_last_chunk_is_done)
{
    CreateAndInitialize();
    host.maxParametersSize = 10;
    host.completeAutomatically = false;
    std::vector<uint8_t> pixels = Pixels(24);
    {
        testing::InSequence sequence;
        ExpectColumns(0, 0, 0, 3);
        ExpectPages(0, 0, 0, 2);
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 10, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 20, 4));
    }

    Write({ 0, 0, 4, 3 }, pixels);
    for (int step = 0; step != 4; ++step)
    {
        host.CompletePending();
        EXPECT_EQ(0, completions);
    }

    host.CompletePending();
    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_new_write_can_start_from_the_completion)
{
    CreateAndInitialize();
    dsitest::AllowAnyCommands(host);
    std::vector<uint8_t> firstPixels = Pixels(2);
    std::vector<uint8_t> secondPixels = Pixels(4);

    display->Write({ 0, 0, 1, 1 }, infra::MakeRange(firstPixels), [this, &secondPixels]()
        {
            display->Write({ 0, 1, 2, 1 }, infra::MakeRange(secondPixels), onDone);
        });
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, write_while_another_is_in_flight_asserts)
{
    CreateAndInitialize();
    host.completeAutomatically = false;
    std::vector<uint8_t> pixels = Pixels(4);
    EXPECT_CALL(host, WriteDcsMock(0x2a, testing::_));
    Write({ 0, 0, 2, 1 }, pixels);

    EXPECT_DEATH(Write({ 0, 1, 2, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_while_an_empty_write_is_pending_asserts)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(2);
    Write({ 5, 5, 0, 0 }, std::vector<uint8_t>());

    EXPECT_DEATH(Write({ 0, 0, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_while_waiting_for_tearing_effect_asserts)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(2);
    Write({ 0, 0, 1, 1 }, pixels);
    ExecuteAllActions();

    EXPECT_DEATH(Write({ 0, 1, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, sleep_while_waiting_for_tearing_effect_asserts)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(2);
    Write({ 0, 0, 1, 1 }, pixels);
    ExecuteAllActions();

    EXPECT_DEATH(display->Sleep(sleepDone), "");
}

TEST_F(MipiDsiDisplayTest, brightness_while_waiting_for_tearing_effect_asserts)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(2);
    Write({ 0, 0, 1, 1 }, pixels);
    ExecuteAllActions();

    EXPECT_DEATH(display->SetBrightness(0x10, sleepDone), "");
}

TEST_F(MipiDsiDisplayTest, write_outside_the_panel_asserts)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(40);

    EXPECT_DEATH(Write({ 470, 0, 20, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_with_too_few_pixels_asserts)
{
    CreateAndInitialize();
    std::vector<uint8_t> pixels = Pixels(20);

    EXPECT_DEATH(Write({ 0, 0, 4, 3 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_while_asleep_asserts)
{
    CreateAndInitialize();
    Sleep();
    std::vector<uint8_t> pixels = Pixels(2);

    EXPECT_DEATH(Write({ 0, 0, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_while_going_to_sleep_asserts)
{
    CreateAndInitialize();
    ExpectDcs(host, 0x28, {});
    display->Sleep(sleepDone);
    std::vector<uint8_t> pixels = Pixels(2);

    EXPECT_DEATH(Write({ 0, 0, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, write_while_waking_asserts)
{
    CreateAndInitialize();
    Sleep();
    ExpectDcs(host, 0x11, {});
    display->Wake(sleepDone);
    std::vector<uint8_t> pixels = Pixels(2);

    EXPECT_DEATH(Write({ 0, 0, 1, 1 }, pixels), "");
}

TEST_F(MipiDsiDisplayTest, sleep_while_a_write_is_in_flight_asserts)
{
    CreateAndInitialize();
    host.completeAutomatically = false;
    std::vector<uint8_t> pixels = Pixels(2);
    EXPECT_CALL(host, WriteDcsMock(0x2a, testing::_));
    Write({ 0, 0, 1, 1 }, pixels);

    EXPECT_DEATH(display->Sleep(sleepDone), "");
}

TEST_F(MipiDsiDisplayTest, brightness_while_a_write_is_in_flight_asserts)
{
    CreateAndInitialize();
    host.completeAutomatically = false;
    std::vector<uint8_t> pixels = Pixels(2);
    EXPECT_CALL(host, WriteDcsMock(0x2a, testing::_));
    Write({ 0, 0, 1, 1 }, pixels);

    EXPECT_DEATH(display->SetBrightness(0x10, sleepDone), "");
}

TEST_F(MipiDsiDisplayTest, the_memory_write_waits_for_the_tearing_effect_edge)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    ExpectColumns(0, 1, 0, 1);
    ExpectPages(0, 2, 0, 2);

    Write({ 1, 2, 1, 1 }, pixels);
    ExecuteAllActions();
    VerifyAll();
    EXPECT_EQ(0, completions);

    ExpectDcs(host, 0x2c, pixels);
    tearingPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, the_tearing_effect_interrupt_is_enabled_on_the_rising_edge_only_while_waiting)
{
    EXPECT_CALL(tearingPinMock, Config(hal::PinConfigType::input));
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPinMock);
    infra::Function<void()> edge;
    std::vector<uint8_t> pixels = Pixels(2);
    ExpectColumns(0, 1, 0, 1);
    ExpectPages(0, 2, 0, 2);
    EXPECT_CALL(tearingPinMock, EnableInterrupt(testing::_, hal::InterruptTrigger::risingEdge, hal::InterruptType::dispatched)).WillOnce(testing::SaveArg<0>(&edge));

    Write({ 1, 2, 1, 1 }, pixels);
    ExecuteAllActions();
    VerifyAll();

    EXPECT_CALL(tearingPinMock, DisableInterrupt());
    ExpectDcs(host, 0x2c, pixels);
    edge();
    ExecuteAllActions();
    VerifyAll();

    DestroyDisplayWithTearingPinMock(1);
}

TEST_F(MipiDsiDisplayTest, a_tearing_effect_edge_cancels_the_timeout)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    dsitest::AllowAnyCommands(host);

    Write({ 1, 2, 1, 1 }, pixels);
    ExecuteAllActions();
    tearingPin.SetStubState(true);
    ForwardTime(std::chrono::milliseconds(200));

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, the_write_proceeds_unsynchronised_after_the_tearing_effect_timeout)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    ExpectColumns(0, 1, 0, 1);
    ExpectPages(0, 2, 0, 2);

    Write({ 1, 2, 1, 1 }, pixels);
    ExecuteAllActions();
    ForwardTime(std::chrono::milliseconds(99));
    VerifyAll();
    EXPECT_EQ(0, completions);

    ExpectDcs(host, 0x2c, pixels);
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_EQ(1, completions);
}

TEST_F(MipiDsiDisplayTest, a_custom_tearing_effect_timeout_is_used)
{
    configuration.timings.tearingEffectTimeout = std::chrono::milliseconds(30);
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    ExpectColumns(0, 1, 0, 1);
    ExpectPages(0, 2, 0, 2);

    Write({ 1, 2, 1, 1 }, pixels);
    ExecuteAllActions();
    ForwardTime(std::chrono::milliseconds(29));
    VerifyAll();

    ExpectDcs(host, 0x2c, pixels);
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiDisplayTest, a_tearing_effect_edge_after_the_timeout_is_ignored)
{
    EXPECT_CALL(tearingPinMock, Config(hal::PinConfigType::input));
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPinMock);
    infra::Function<void()> edge;
    std::vector<uint8_t> pixels = Pixels(2);
    dsitest::AllowAnyCommands(host);
    EXPECT_CALL(tearingPinMock, EnableInterrupt(testing::_, testing::_, testing::_)).WillOnce(testing::SaveArg<0>(&edge));
    EXPECT_CALL(tearingPinMock, DisableInterrupt());

    Write({ 1, 2, 1, 1 }, pixels);
    ForwardTime(std::chrono::milliseconds(100));
    VerifyAll();
    EXPECT_EQ(1, completions);

    edge();
    ExecuteAllActions();

    DestroyDisplayWithTearingPinMock(1);
}

TEST_F(MipiDsiDisplayTest, a_multi_chunk_write_waits_for_tearing_effect_once)
{
    EXPECT_CALL(tearingPinMock, Config(hal::PinConfigType::input));
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPinMock);
    host.maxParametersSize = 10;
    infra::Function<void()> edge;
    std::vector<uint8_t> pixels = Pixels(24);
    ExpectColumns(0, 0, 0, 3);
    ExpectPages(0, 0, 0, 2);
    EXPECT_CALL(tearingPinMock, EnableInterrupt(testing::_, testing::_, testing::_)).WillOnce(testing::SaveArg<0>(&edge));

    Write({ 0, 0, 4, 3 }, pixels);
    ExecuteAllActions();
    VerifyAll();

    EXPECT_CALL(tearingPinMock, DisableInterrupt());
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x2c, Slice(pixels, 0, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 10, 10));
        ExpectDcs(host, 0x3c, Slice(pixels, 20, 4));
    }
    edge();
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
    VerifyAll();
    DestroyDisplayWithTearingPinMock(1);
}

TEST_F(MipiDsiDisplayTest, each_write_waits_for_its_own_tearing_effect_edge)
{
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPin);
    std::vector<uint8_t> pixels = Pixels(2);
    dsitest::AllowAnyCommands(host);

    Write({ 0, 0, 1, 1 }, pixels);
    ExecuteAllActions();
    tearingPin.SetStubState(true);
    ExecuteAllActions();
    EXPECT_EQ(1, completions);
    VerifyAll();

    tearingPin.SetStubState(false);
    tearingPin.SetStubState(true);
    ExpectColumns(0, 0, 0, 0);
    ExpectPages(0, 1, 0, 1);

    Write({ 0, 1, 1, 1 }, pixels);
    ExecuteAllActions();
    VerifyAll();
    EXPECT_EQ(1, completions);

    ExpectDcs(host, 0x2c, pixels);
    tearingPin.SetStubState(false);
    tearingPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(2, completions);
}

TEST_F(MipiDsiDisplayTest, destroying_the_display_disables_the_tearing_effect_interrupt)
{
    EXPECT_CALL(tearingPinMock, Config(hal::PinConfigType::input));
    CreateAndInitialize(hal::PixelFormat::rgb565Swapped, tearingPinMock);

    DestroyDisplayWithTearingPinMock(1);
}

TEST_P(MipiDsiDisplayPlacementTest, packed_pixels_land_in_the_window_for_every_chunk_size)
{
    std::vector<uint8_t> pixels = Pixels(4 * 2 * PixelBytes());

    display->Write({ 1, 1, 4, 2 }, infra::MakeRange(pixels), onDone);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
    for (uint16_t row = 0; row != 2; ++row)
        for (uint16_t column = 0; column != 4; ++column)
            ExpectPixel(static_cast<uint16_t>(1 + column), static_cast<uint16_t>(1 + row), pixels, (row * 4 + column) * PixelBytes());

    ExpectUntouched(0, 0);
    ExpectUntouched(5, 3);
    ExpectUntouched(0, 1);
}

TEST_P(MipiDsiDisplayPlacementTest, strided_pixels_land_in_the_window_for_every_chunk_size)
{
    std::size_t stride = 5 * PixelBytes();
    std::vector<uint8_t> pixels = Pixels(2 * stride + 3 * PixelBytes());

    display->WriteWithStride({ 2, 0, 3, 3 }, infra::MakeRange(pixels), stride, onDone);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
    for (uint16_t row = 0; row != 3; ++row)
        for (uint16_t column = 0; column != 3; ++column)
            ExpectPixel(static_cast<uint16_t>(2 + column), row, pixels, row * stride + column * PixelBytes());

    ExpectUntouched(1, 0);
    ExpectUntouched(5, 0);
    ExpectUntouched(2, 3);
}

INSTANTIATE_TEST_SUITE_P(formats_and_chunk_sizes, MipiDsiDisplayPlacementTest, testing::Combine(testing::Values(hal::PixelFormat::rgb565Swapped, hal::PixelFormat::rgb888), testing::Range<std::size_t>(4, 21)));
