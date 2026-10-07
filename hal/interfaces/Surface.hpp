#ifndef HAL_SURFACE_HPP
#define HAL_SURFACE_HPP

#include "hal/interfaces/Display.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    using Argb8888 = uint32_t;

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

    uint8_t BitsPerPixel(SurfaceFormat format);
    bool HasAlpha(SurfaceFormat format);
    bool IsIndexed(SurfaceFormat format);
    std::size_t BytesPerRow(uint16_t width, SurfaceFormat format);

    uint32_t ToPixel(Argb8888 color, SurfaceFormat format);

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

    bool IsValidSurface(DisplaySize size, SurfaceFormat format, uint32_t strideInBytes, std::size_t memorySize);

    template<class Range>
    bool IsValidSurface(const BasicSurface<Range>& surface)
    {
        return IsValidSurface(surface.size, surface.format, surface.strideInBytes, surface.memory.size());
    }

    struct SurfaceWindow
    {
        std::size_t offset;
        std::size_t length;
    };

    SurfaceWindow WindowOf(DisplaySize size, SurfaceFormat format, uint32_t strideInBytes, const DisplayArea& area);

    template<class Range>
    BasicSurface<Range> SubSurface(const BasicSurface<Range>& surface, const DisplayArea& area)
    {
        SurfaceWindow window = WindowOf(surface.size, surface.format, surface.strideInBytes, area);

        return { infra::Head(infra::DiscardHead(surface.memory, window.offset), window.length), DisplaySize{ area.width, area.height }, surface.strideInBytes, surface.format };
    }
}

#endif
