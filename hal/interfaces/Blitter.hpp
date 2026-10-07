#ifndef HAL_BLITTER_HPP
#define HAL_BLITTER_HPP

#include "hal/interfaces/Surface.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    enum class BlitOperation : uint8_t
    {
        fill,
        copy,
        blend
    };

    struct BlendSource
    {
        ConstSurface surface;
        uint8_t alpha{ 255 };
        Argb8888 color{ 0 };
    };

    class Blitter
    {
    protected:
        Blitter() = default;
        Blitter(const Blitter& other) = delete;
        Blitter& operator=(const Blitter& other) = delete;
        ~Blitter() = default;

    public:
        virtual bool Supports(BlitOperation operation, SurfaceFormat source, SurfaceFormat destination) const = 0;

        virtual void Fill(const Surface& destination, Argb8888 color, const infra::Function<void()>& onDone) = 0;

        virtual void Copy(const ConstSurface& source, const Surface& destination, const infra::Function<void()>& onDone) = 0;

        virtual void Blend(const BlendSource& foreground, const ConstSurface& background, const Surface& destination, const infra::Function<void()>& onDone) = 0;
    };
}

#endif
