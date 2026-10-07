#include "hal/interfaces/CameraFormat.hpp"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>

TEST(CameraFormatTest, bytes_per_pixel_matches_each_format)
{
    EXPECT_EQ(std::size_t(1), hal::BytesPerPixel(hal::CameraPixelFormat::grey8));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::CameraPixelFormat::rgb565));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::CameraPixelFormat::rgb565Swapped));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::CameraPixelFormat::yuv422Yuyv));
    EXPECT_EQ(std::size_t(2), hal::BytesPerPixel(hal::CameraPixelFormat::yuv422Uyvy));
}

TEST(CameraFormatTest, jpeg_is_compressed_and_has_no_bytes_per_pixel)
{
    EXPECT_TRUE(hal::IsCompressed(hal::CameraPixelFormat::jpeg));
    EXPECT_EQ(std::size_t(0), hal::BytesPerPixel(hal::CameraPixelFormat::jpeg));
}

TEST(CameraFormatTest, frame_size_of_qvga_rgb565_is_153600)
{
    constexpr hal::CameraFormat qvga{ 320, 240, hal::CameraPixelFormat::rgb565 };
    EXPECT_EQ(std::size_t(153600), hal::FrameSizeInBytes(qvga));
}

TEST(CameraFormatTest, frame_size_of_vga_yuyv_is_614400)
{
    constexpr hal::CameraFormat vga{ 640, 480, hal::CameraPixelFormat::yuv422Yuyv };
    EXPECT_EQ(std::size_t(614400), hal::FrameSizeInBytes(vga));
}

TEST(CameraFormatTest, frame_size_of_a_compressed_format_is_zero)
{
    constexpr hal::CameraFormat jpeg{ 320, 240, hal::CameraPixelFormat::jpeg };
    EXPECT_EQ(std::size_t(0), hal::FrameSizeInBytes(jpeg));
}

TEST(CameraFormatTest, buffer_of_exactly_the_frame_size_is_valid)
{
    constexpr hal::CameraFormat format{ 4, 3, hal::CameraPixelFormat::rgb565 };
    EXPECT_TRUE(hal::IsValidFrameBuffer(format, 24));
}

TEST(CameraFormatTest, larger_buffer_is_valid)
{
    constexpr hal::CameraFormat format{ 4, 3, hal::CameraPixelFormat::rgb565 };
    EXPECT_TRUE(hal::IsValidFrameBuffer(format, 32));
}

TEST(CameraFormatTest, buffer_one_byte_short_is_invalid)
{
    constexpr hal::CameraFormat format{ 4, 3, hal::CameraPixelFormat::rgb565 };
    EXPECT_FALSE(hal::IsValidFrameBuffer(format, 23));
}

TEST(CameraFormatTest, frame_without_width_or_height_is_invalid)
{
    EXPECT_FALSE(hal::IsValidFrameBuffer({ 0, 240, hal::CameraPixelFormat::rgb565 }, 1024));
    EXPECT_FALSE(hal::IsValidFrameBuffer({ 320, 0, hal::CameraPixelFormat::rgb565 }, 1024));
}

TEST(CameraFormatTest, compressed_frame_accepts_any_non_empty_buffer)
{
    constexpr hal::CameraFormat jpeg{ 320, 240, hal::CameraPixelFormat::jpeg };
    EXPECT_TRUE(hal::IsValidFrameBuffer(jpeg, 1));
    EXPECT_TRUE(hal::IsValidFrameBuffer(jpeg, 65536));
}

TEST(CameraFormatTest, compressed_frame_rejects_an_empty_buffer)
{
    constexpr hal::CameraFormat jpeg{ 320, 240, hal::CameraPixelFormat::jpeg };
    EXPECT_FALSE(hal::IsValidFrameBuffer(jpeg, 0));
}

TEST(CameraFormatTest, formats_compare_by_every_field)
{
    constexpr hal::CameraFormat a{ 320, 240, hal::CameraPixelFormat::rgb565 };
    constexpr hal::CameraFormat b{ 320, 240, hal::CameraPixelFormat::rgb565 };
    constexpr hal::CameraFormat differentWidth{ 640, 240, hal::CameraPixelFormat::rgb565 };
    constexpr hal::CameraFormat differentHeight{ 320, 480, hal::CameraPixelFormat::rgb565 };
    constexpr hal::CameraFormat differentPixelFormat{ 320, 240, hal::CameraPixelFormat::grey8 };

    EXPECT_EQ(a, b);
    EXPECT_NE(a, differentWidth);
    EXPECT_NE(a, differentHeight);
    EXPECT_NE(a, differentPixelFormat);
}

TEST(CameraFormatTest, largest_dimensions_do_not_overflow_the_validation)
{
    constexpr hal::CameraFormat maxSize{ 65535, 65535, hal::CameraPixelFormat::rgb565 };
    constexpr auto expected = static_cast<uint64_t>(65535) * 65535 * 2;
    EXPECT_FALSE(hal::IsValidFrameBuffer(maxSize, static_cast<std::size_t>(expected - 1)));
    EXPECT_TRUE(hal::IsValidFrameBuffer(maxSize, static_cast<std::size_t>(expected)));
}
