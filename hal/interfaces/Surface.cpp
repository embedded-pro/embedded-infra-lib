#include "hal/interfaces/Surface.hpp"

namespace hal
{
    uint8_t BitsPerPixel(SurfaceFormat format)
    {
        switch (format)
        {
            case SurfaceFormat::argb8888:
                return 32;
            case SurfaceFormat::rgb888:
                return 24;
            case SurfaceFormat::rgb565:
            case SurfaceFormat::argb1555:
            case SurfaceFormat::argb4444:
            case SurfaceFormat::al88:
                return 16;
            case SurfaceFormat::l8:
            case SurfaceFormat::al44:
            case SurfaceFormat::a8:
                return 8;
            case SurfaceFormat::a4:
                return 4;
        }

        return 0;
    }

    bool HasAlpha(SurfaceFormat format)
    {
        switch (format)
        {
            case SurfaceFormat::argb8888:
            case SurfaceFormat::argb1555:
            case SurfaceFormat::argb4444:
            case SurfaceFormat::al44:
            case SurfaceFormat::al88:
            case SurfaceFormat::a8:
            case SurfaceFormat::a4:
                return true;
            case SurfaceFormat::rgb888:
            case SurfaceFormat::rgb565:
            case SurfaceFormat::l8:
                return false;
        }

        return false;
    }

    bool IsIndexed(SurfaceFormat format)
    {
        return format == SurfaceFormat::l8 || format == SurfaceFormat::al44 || format == SurfaceFormat::al88;
    }

    std::size_t BytesPerRow(uint16_t width, SurfaceFormat format)
    {
        return (std::size_t{ width } * BitsPerPixel(format) + 7) / 8;
    }

    uint32_t ToPixel(Argb8888 color, SurfaceFormat format)
    {
        uint32_t alpha = (color >> 24) & 0xff;
        uint32_t red = (color >> 16) & 0xff;
        uint32_t green = (color >> 8) & 0xff;
        uint32_t blue = color & 0xff;

        switch (format)
        {
            case SurfaceFormat::argb8888:
                return color;
            case SurfaceFormat::rgb888:
                return color & 0xffffff;
            case SurfaceFormat::rgb565:
                return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
            case SurfaceFormat::argb1555:
                return ((alpha >> 7) << 15) | ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3);
            case SurfaceFormat::argb4444:
                return ((alpha >> 4) << 12) | ((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4);
            case SurfaceFormat::l8:
            case SurfaceFormat::al44:
            case SurfaceFormat::al88:
            case SurfaceFormat::a8:
            case SurfaceFormat::a4:
                break;
        }

        really_assert(false);
        return 0;
    }

    bool IsValidSurface(DisplaySize size, SurfaceFormat format, uint32_t strideInBytes, std::size_t memorySize)
    {
        if (size.width == 0 || size.height == 0)
            return true;

        std::size_t rowBytes = BytesPerRow(size.width, format);
        return strideInBytes >= rowBytes && memorySize >= (size.height - 1) * std::size_t{ strideInBytes } + rowBytes;
    }

    SurfaceWindow WindowOf(DisplaySize size, SurfaceFormat format, uint32_t strideInBytes, const DisplayArea& area)
    {
        really_assert(uint32_t{ area.x } + area.width <= size.width && uint32_t{ area.y } + area.height <= size.height);
        really_assert(std::size_t{ area.x } * BitsPerPixel(format) % 8 == 0);

        std::size_t offset = std::size_t{ area.y } * strideInBytes + std::size_t{ area.x } * BitsPerPixel(format) / 8;
        std::size_t length = area.width == 0 || area.height == 0 ? 0 : (area.height - 1) * std::size_t{ strideInBytes } + BytesPerRow(area.width, format);

        return { offset, length };
    }
}
