#ifndef HAL_SURFACE_HPP
#define HAL_SURFACE_HPP

#include "hal/interfaces/Display.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    // 0xAARRGGBB with straight, not premultiplied, alpha
    using Argb8888 = uint32_t;

    // The layout of pixels in memory, as a scan-out engine or a 2D accelerator reads and writes them.
    // hal::PixelFormat describes the bytes a display bus expects instead.
    enum class SurfaceFormat : uint8_t
    {
        argb8888,
        rgb888,
        rgb565,
        argb1555,
        argb4444,
        l8,
        al44,
        al88,
        a8,
        a4
    };

    constexpr uint8_t BitsPerPixel(SurfaceFormat format)
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

    constexpr bool HasAlpha(SurfaceFormat format)
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

    constexpr bool IsIndexed(SurfaceFormat format)
    {
        return format == SurfaceFormat::l8 || format == SurfaceFormat::al44 || format == SurfaceFormat::al88;
    }

    constexpr std::size_t BytesPerRow(uint16_t width, SurfaceFormat format)
    {
        return (std::size_t{ width } * BitsPerPixel(format) + 7) / 8;
    }

    // Converts to the pixel value of a format that holds colour directly. Alpha-only and indexed formats have no such value
    uint32_t ToPixel(Argb8888 color, SurfaceFormat format);

    // A non-owning view of a pixel buffer. Rows are strideInBytes apart, which may exceed the bytes a row needs
    template<class Range>
    struct BasicSurface
    {
        Range memory;
        DisplaySize size;
        uint32_t strideInBytes;
        SurfaceFormat format;
    };

    using Surface = BasicSurface<infra::ByteRange>;
    using ConstSurface = BasicSurface<infra::ConstByteRange>;

    inline ConstSurface AsConst(const Surface& surface)
    {
        return { surface.memory, surface.size, surface.strideInBytes, surface.format };
    }

    template<class Range>
    bool IsValidSurface(const BasicSurface<Range>& surface)
    {
        if (surface.size.width == 0 || surface.size.height == 0)
            return true;

        std::size_t rowBytes = BytesPerRow(surface.size.width, surface.format);
        return surface.strideInBytes >= rowBytes && surface.memory.size() >= (surface.size.height - 1) * std::size_t{ surface.strideInBytes } + rowBytes;
    }

    // The window keeps the stride and the format of the surface. The left edge must fall on a byte boundary
    template<class Range>
    BasicSurface<Range> SubSurface(const BasicSurface<Range>& surface, const DisplayArea& area)
    {
        really_assert(uint32_t{ area.x } + area.width <= surface.size.width && uint32_t{ area.y } + area.height <= surface.size.height);
        really_assert(std::size_t{ area.x } * BitsPerPixel(surface.format) % 8 == 0);

        std::size_t offset = std::size_t{ area.y } * surface.strideInBytes + std::size_t{ area.x } * BitsPerPixel(surface.format) / 8;
        std::size_t length = area.width == 0 || area.height == 0 ? 0 : (area.height - 1) * std::size_t{ surface.strideInBytes } + BytesPerRow(area.width, surface.format);

        return { infra::Head(infra::DiscardHead(surface.memory, offset), length), DisplaySize{ area.width, area.height }, surface.strideInBytes, surface.format };
    }
}

#endif
