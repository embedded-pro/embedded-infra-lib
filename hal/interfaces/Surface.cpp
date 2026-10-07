#include "hal/interfaces/Surface.hpp"

namespace hal
{
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
}
