#include "hal/interfaces/Surface.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr hal::DisplaySize qvga{ 240, 320 };
    constexpr uint32_t rgb565Stride = 480;

    struct FormatProperties
    {
        hal::SurfaceFormat format;
        uint8_t bitsPerPixel;
        bool hasAlpha;
        bool isIndexed;
    };

    const std::array<FormatProperties, 10> formatProperties{ {
        { hal::SurfaceFormat::argb8888, 32, true, false },
        { hal::SurfaceFormat::rgb888, 24, false, false },
        { hal::SurfaceFormat::rgb565, 16, false, false },
        { hal::SurfaceFormat::argb1555, 16, true, false },
        { hal::SurfaceFormat::argb4444, 16, true, false },
        { hal::SurfaceFormat::l8, 8, false, true },
        { hal::SurfaceFormat::al44, 8, true, true },
        { hal::SurfaceFormat::al88, 16, true, true },
        { hal::SurfaceFormat::a8, 8, true, false },
        { hal::SurfaceFormat::a4, 4, true, false },
    } };

    class SurfaceTest
        : public testing::Test
    {
    public:
        hal::Surface Rgb565()
        {
            return { buffer, qvga, rgb565Stride, hal::SurfaceFormat::rgb565 };
        }

        std::array<uint8_t, rgb565Stride * 320> buffer{};
    };
}

TEST(SurfaceFormatTest, bits_per_pixel_matches_each_format)
{
    for (const FormatProperties& properties : formatProperties)
        EXPECT_EQ(properties.bitsPerPixel, hal::BitsPerPixel(properties.format)) << static_cast<int>(properties.format);
}

TEST(SurfaceFormatTest, formats_with_alpha)
{
    for (const FormatProperties& properties : formatProperties)
        EXPECT_EQ(properties.hasAlpha, hal::HasAlpha(properties.format)) << static_cast<int>(properties.format);
}

TEST(SurfaceFormatTest, indexed_formats)
{
    for (const FormatProperties& properties : formatProperties)
        EXPECT_EQ(properties.isIndexed, hal::IsIndexed(properties.format)) << static_cast<int>(properties.format);
}

TEST(SurfaceFormatTest, bytes_per_row_rounds_up_to_whole_bytes)
{
    EXPECT_EQ(std::size_t(8), hal::BytesPerRow(4, hal::SurfaceFormat::rgb565));
    EXPECT_EQ(std::size_t(9), hal::BytesPerRow(3, hal::SurfaceFormat::rgb888));
    EXPECT_EQ(std::size_t(3), hal::BytesPerRow(5, hal::SurfaceFormat::a4));
    EXPECT_EQ(std::size_t(0), hal::BytesPerRow(0, hal::SurfaceFormat::argb8888));
}

TEST(SurfaceColorTest, argb8888_is_unchanged)
{
    EXPECT_EQ(0x80ff8001u, hal::ToPixel(0x80ff8001, hal::SurfaceFormat::argb8888));
}

TEST(SurfaceColorTest, rgb888_drops_the_alpha)
{
    EXPECT_EQ(0xff8001u, hal::ToPixel(0x80ff8001, hal::SurfaceFormat::rgb888));
}

TEST(SurfaceColorTest, rgb565_keeps_the_top_bits_of_each_channel)
{
    EXPECT_EQ(0xfc00u, hal::ToPixel(0xffff8000, hal::SurfaceFormat::rgb565));
    EXPECT_EQ(0xffffu, hal::ToPixel(0xffffffff, hal::SurfaceFormat::rgb565));
    EXPECT_EQ(0x0000u, hal::ToPixel(0xff000000, hal::SurfaceFormat::rgb565));
}

TEST(SurfaceColorTest, argb1555_keeps_one_alpha_bit)
{
    EXPECT_EQ(0xfe00u, hal::ToPixel(0xffff8000, hal::SurfaceFormat::argb1555));
    EXPECT_EQ(0x7e00u, hal::ToPixel(0x7fff8000, hal::SurfaceFormat::argb1555));
}

TEST(SurfaceColorTest, argb4444_keeps_four_bits_of_each_channel)
{
    EXPECT_EQ(0x8f80u, hal::ToPixel(0x80ff8000, hal::SurfaceFormat::argb4444));
}

TEST(SurfaceColorTest, indexed_and_alpha_only_formats_have_no_pixel_value)
{
    EXPECT_DEATH(hal::ToPixel(0xffffffff, hal::SurfaceFormat::l8), "");
    EXPECT_DEATH(hal::ToPixel(0xffffffff, hal::SurfaceFormat::a8), "");
}

TEST_F(SurfaceTest, a_surface_that_holds_all_its_rows_is_valid)
{
    EXPECT_TRUE(hal::IsValidSurface(Rgb565()));
}

TEST_F(SurfaceTest, a_const_surface_is_validated_like_a_surface)
{
    hal::ConstSurface surface = hal::AsConst(Rgb565());

    EXPECT_TRUE(hal::IsValidSurface(surface));

    surface.strideInBytes = 478;
    EXPECT_FALSE(hal::IsValidSurface(surface));
}

TEST_F(SurfaceTest, a_stride_shorter_than_a_row_is_invalid)
{
    hal::Surface surface = Rgb565();
    surface.strideInBytes = 478;

    EXPECT_FALSE(hal::IsValidSurface(surface));
}

TEST_F(SurfaceTest, memory_shorter_than_the_last_row_is_invalid)
{
    hal::Surface surface = Rgb565();
    surface.memory = infra::Head(surface.memory, buffer.size() - 1);

    EXPECT_FALSE(hal::IsValidSurface(surface));
}

TEST_F(SurfaceTest, the_last_row_needs_no_padding)
{
    hal::Surface surface = Rgb565();
    surface.size = { 100, 320 };
    surface.memory = infra::Head(surface.memory, 319 * rgb565Stride + 200);

    EXPECT_TRUE(hal::IsValidSurface(surface));
}

TEST_F(SurfaceTest, an_empty_surface_is_valid)
{
    hal::Surface surface{ infra::ByteRange(), { 0, 10 }, 0, hal::SurfaceFormat::rgb565 };

    EXPECT_TRUE(hal::IsValidSurface(surface));
}

TEST_F(SurfaceTest, sub_surface_starts_at_the_window_and_keeps_stride_and_format)
{
    hal::Surface window = hal::SubSurface(Rgb565(), { 10, 20, 100, 50 });

    EXPECT_EQ(buffer.data() + 20 * rgb565Stride + 10 * 2, window.memory.begin());
    EXPECT_EQ((hal::DisplaySize{ 100, 50 }), window.size);
    EXPECT_EQ(rgb565Stride, window.strideInBytes);
    EXPECT_EQ(hal::SurfaceFormat::rgb565, window.format);
}

TEST_F(SurfaceTest, sub_surface_ends_with_the_last_pixel_of_the_window)
{
    hal::Surface window = hal::SubSurface(Rgb565(), { 10, 20, 100, 50 });

    EXPECT_EQ(std::size_t(49 * rgb565Stride + 200), window.memory.size());
    EXPECT_TRUE(hal::IsValidSurface(window));
}

TEST_F(SurfaceTest, sub_surface_of_the_whole_surface_is_the_surface)
{
    hal::Surface window = hal::SubSurface(Rgb565(), { 0, 0, 240, 320 });

    EXPECT_EQ(buffer.data(), window.memory.begin());
    EXPECT_EQ(buffer.size(), window.memory.size());
}

TEST_F(SurfaceTest, sub_surface_of_a_const_surface_is_const)
{
    hal::ConstSurface window = hal::SubSurface(hal::AsConst(Rgb565()), { 4, 1, 8, 2 });

    EXPECT_EQ(buffer.data() + rgb565Stride + 8, window.memory.begin());
    EXPECT_EQ(std::size_t(rgb565Stride + 16), window.memory.size());
}

TEST_F(SurfaceTest, sub_surface_scales_the_offset_by_the_pixel_size)
{
    hal::Surface surface{ buffer, { 16, 8 }, 64, hal::SurfaceFormat::argb8888 };

    hal::Surface window = hal::SubSurface(surface, { 3, 2, 4, 2 });

    EXPECT_EQ(buffer.data() + 2 * 64 + 3 * 4, window.memory.begin());
}

TEST_F(SurfaceTest, sub_surface_of_four_bit_pixels_starts_on_a_byte)
{
    hal::Surface surface{ buffer, { 16, 8 }, 8, hal::SurfaceFormat::a4 };

    hal::Surface window = hal::SubSurface(surface, { 4, 1, 4, 2 });

    EXPECT_EQ(buffer.data() + 8 + 2, window.memory.begin());
    EXPECT_EQ(std::size_t(8 + 2), window.memory.size());
}

TEST_F(SurfaceTest, sub_surface_of_four_bit_pixels_cannot_start_inside_a_byte)
{
    hal::Surface surface{ buffer, { 16, 8 }, 8, hal::SurfaceFormat::a4 };

    EXPECT_DEATH(hal::SubSurface(surface, { 3, 0, 4, 2 }), "");
}

TEST_F(SurfaceTest, an_empty_window_has_no_memory)
{
    hal::Surface window = hal::SubSurface(Rgb565(), { 10, 20, 0, 50 });

    EXPECT_TRUE(window.memory.empty());
}

TEST_F(SurfaceTest, sub_surface_outside_the_surface_asserts)
{
    EXPECT_DEATH(hal::SubSurface(Rgb565(), { 200, 0, 41, 1 }), "");
    EXPECT_DEATH(hal::SubSurface(Rgb565(), { 0, 300, 1, 21 }), "");
}
