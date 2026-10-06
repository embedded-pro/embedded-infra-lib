#include "hal/interfaces/test_doubles/DisplayStub.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr hal::DisplaySize displaySize{ 4, 3 };
    constexpr std::size_t bytesPerPixel = 2;
    constexpr std::size_t framebufferSize = 4 * 3 * bytesPerPixel;

    std::array<uint8_t, 2> Pixel(uint8_t first, uint8_t second)
    {
        return { first, second };
    }

    class DisplayTest
        : public testing::Test
    {
    public:
        void ExpectWrite(const hal::DisplayArea& area, std::size_t strideInBytes)
        {
            EXPECT_CALL(stub, WriteWithStride(area, testing::_, strideInBytes, testing::_));
        }

        testing::StrictMock<hal::DisplayStub::WithStorage<framebufferSize>> stub{ displaySize, hal::PixelFormat::rgb565 };
        hal::Display& display{ stub };
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
    };
}

TEST(DisplayFormatTest, bytes_per_pixel_matches_each_format)
{
    EXPECT_EQ(std::size_t(1), hal::BytesPerPixel(hal::PixelFormat::grey8));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::PixelFormat::rgb565));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::PixelFormat::rgb565Swapped));
    EXPECT_EQ(std::size_t(3), hal::BytesPerPixel(hal::PixelFormat::rgb888));
}

TEST(DisplayValidationTest, area_inside_the_display_with_a_packed_buffer_is_valid)
{
    std::array<uint8_t, 12> pixels{};

    EXPECT_TRUE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 1, 1, 2, 2 }, pixels, 4));
}

TEST(DisplayValidationTest, area_covering_the_whole_display_is_valid)
{
    std::array<uint8_t, framebufferSize> pixels{};

    EXPECT_TRUE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 0, 0, 4, 3 }, pixels, 8));
}

TEST(DisplayValidationTest, area_exceeding_the_width_is_invalid)
{
    std::array<uint8_t, framebufferSize> pixels{};

    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 3, 0, 2, 1 }, pixels, 4));
}

TEST(DisplayValidationTest, area_exceeding_the_height_is_invalid)
{
    std::array<uint8_t, framebufferSize> pixels{};

    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 0, 2, 1, 2 }, pixels, 2));
}

TEST(DisplayValidationTest, offset_and_size_that_overflow_16_bits_are_invalid)
{
    std::array<uint8_t, framebufferSize> pixels{};

    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 65535, 0, 2, 1 }, pixels, 4));
}

TEST(DisplayValidationTest, buffer_smaller_than_the_area_is_invalid)
{
    std::array<uint8_t, 7> pixels{};

    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 0, 0, 2, 2 }, pixels, 4));
}

TEST(DisplayValidationTest, buffer_that_ends_with_the_last_pixel_of_the_last_row_is_valid_with_a_stride)
{
    std::array<uint8_t, 2 * 8 + 4> pixels{};

    EXPECT_TRUE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 0, 0, 2, 3 }, pixels, 8));
}

TEST(DisplayValidationTest, stride_shorter_than_a_row_is_invalid)
{
    std::array<uint8_t, framebufferSize> pixels{};

    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 0, 0, 2, 2 }, pixels, 3));
}

TEST(DisplayValidationTest, empty_area_is_valid_without_pixels)
{
    EXPECT_TRUE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 1, 1, 0, 0 }, infra::ConstByteRange(), 0));
}

TEST(DisplayValidationTest, empty_area_outside_the_display_is_invalid)
{
    EXPECT_FALSE(hal::IsValidDisplayWrite(displaySize, hal::PixelFormat::rgb565, { 5, 0, 0, 0 }, infra::ConstByteRange(), 0));
}

TEST_F(DisplayTest, size_and_format_are_reported)
{
    EXPECT_EQ(displaySize, display.Size());
    EXPECT_EQ(hal::PixelFormat::rgb565, display.Format());
}

TEST_F(DisplayTest, packed_write_uses_the_row_length_as_stride)
{
    std::array<uint8_t, 12> pixels{};
    ExpectWrite({ 0, 0, 3, 2 }, 6);

    display.Write({ 0, 0, 3, 2 }, pixels, onDone);
}

TEST_F(DisplayTest, packed_write_derives_the_stride_from_the_pixel_format)
{
    testing::StrictMock<hal::DisplayStub::WithStorage<4 * 3 * 3>> rgb888Stub{ displaySize, hal::PixelFormat::rgb888 };
    hal::Display& rgb888Display{ rgb888Stub };
    std::array<uint8_t, 6> pixels{};
    EXPECT_CALL(rgb888Stub, WriteWithStride(hal::DisplayArea{ 0, 0, 2, 1 }, testing::_, 6, testing::_));

    rgb888Display.Write({ 0, 0, 2, 1 }, pixels, onDone);
}

TEST_F(DisplayTest, packed_write_hands_the_same_pixels_to_the_implementation)
{
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).WillOnce([&](const hal::DisplayArea&, infra::ConstByteRange received, std::size_t, const infra::Function<void()>&)
        {
            EXPECT_EQ(pixels.data(), received.begin());
            EXPECT_EQ(pixels.size(), received.size());
        });

    display.Write({ 0, 0, 2, 1 }, pixels, onDone);
}

TEST_F(DisplayTest, write_stores_the_pixels_at_the_area_position)
{
    std::array<uint8_t, 8> pixels{ 1, 2, 3, 4, 5, 6, 7, 8 };
    ExpectWrite({ 1, 1, 2, 2 }, 4);

    display.Write({ 1, 1, 2, 2 }, pixels, onDone);

    EXPECT_EQ(Pixel(1, 2), stub.PixelAt(1, 1));
    EXPECT_EQ(Pixel(3, 4), stub.PixelAt(2, 1));
    EXPECT_EQ(Pixel(5, 6), stub.PixelAt(1, 2));
    EXPECT_EQ(Pixel(7, 8), stub.PixelAt(2, 2));
}

TEST_F(DisplayTest, write_leaves_pixels_outside_the_area_untouched)
{
    std::array<uint8_t, 4> pixels{ 9, 9, 9, 9 };
    ExpectWrite({ 1, 1, 2, 1 }, 4);

    display.Write({ 1, 1, 2, 1 }, pixels, onDone);

    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(0, 1));
    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(3, 1));
    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(1, 0));
    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(1, 2));
}

TEST_F(DisplayTest, write_with_a_stride_skips_the_padding_between_rows)
{
    std::array<uint8_t, 12> pixels{ 1, 2, 3, 4, 0xee, 0xee, 5, 6, 7, 8, 0xee, 0xee };
    EXPECT_CALL(stub, WriteWithStride(hal::DisplayArea{ 0, 0, 2, 2 }, testing::_, 6, testing::_));

    display.WriteWithStride({ 0, 0, 2, 2 }, pixels, 6, onDone);

    EXPECT_EQ(Pixel(1, 2), stub.PixelAt(0, 0));
    EXPECT_EQ(Pixel(3, 4), stub.PixelAt(1, 0));
    EXPECT_EQ(Pixel(5, 6), stub.PixelAt(0, 1));
    EXPECT_EQ(Pixel(7, 8), stub.PixelAt(1, 1));
    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(2, 0));
}

TEST_F(DisplayTest, write_does_not_complete_before_the_implementation_is_done)
{
    std::array<uint8_t, 4> pixels{};
    ExpectWrite({ 0, 0, 2, 1 }, 4);

    display.Write({ 0, 0, 2, 1 }, pixels, onDone);

    EXPECT_EQ(0, completions);
    EXPECT_TRUE(stub.WritePending());
}

TEST_F(DisplayTest, write_completes_once_when_the_implementation_is_done)
{
    std::array<uint8_t, 4> pixels{};
    ExpectWrite({ 0, 0, 2, 1 }, 4);
    display.Write({ 0, 0, 2, 1 }, pixels, onDone);

    stub.CompleteWrite();

    EXPECT_EQ(1, completions);
    EXPECT_FALSE(stub.WritePending());
}

TEST_F(DisplayTest, a_new_write_is_accepted_after_completion)
{
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).Times(2);
    display.Write({ 0, 0, 2, 1 }, pixels, onDone);
    stub.CompleteWrite();

    display.Write({ 0, 1, 2, 1 }, pixels, onDone);
    stub.CompleteWrite();

    EXPECT_EQ(2, completions);
}

TEST_F(DisplayTest, a_new_write_can_be_started_from_the_completion_callback)
{
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).Times(2);
    display.Write({ 0, 0, 2, 1 }, pixels, [&]()
        {
            display.Write({ 0, 1, 2, 1 }, pixels, onDone);
        });

    stub.CompleteWrite();
    stub.CompleteWrite();

    EXPECT_EQ(1, completions);
}

TEST_F(DisplayTest, empty_area_changes_nothing_and_still_completes)
{
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_));

    display.Write({ 2, 2, 0, 0 }, infra::ConstByteRange(), onDone);
    stub.CompleteWrite();

    EXPECT_EQ(1, completions);
    EXPECT_EQ(Pixel(0, 0), stub.PixelAt(2, 2));
}

TEST_F(DisplayTest, write_while_another_is_in_flight_asserts)
{
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).Times(testing::AtLeast(1));
    display.Write({ 0, 0, 2, 1 }, pixels, onDone);

    EXPECT_DEATH(display.Write({ 0, 1, 2, 1 }, pixels, onDone), "");
}

TEST_F(DisplayTest, write_outside_the_display_asserts)
{
    std::array<uint8_t, 4> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).Times(testing::AtLeast(0));

    EXPECT_DEATH(display.Write({ 3, 0, 2, 1 }, pixels, onDone), "");
}

TEST_F(DisplayTest, write_with_too_few_pixels_asserts)
{
    std::array<uint8_t, 2> pixels{};
    EXPECT_CALL(stub, WriteWithStride(testing::_, testing::_, testing::_, testing::_)).Times(testing::AtLeast(0));

    EXPECT_DEATH(display.Write({ 0, 0, 2, 1 }, pixels, onDone), "");
}
