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
        Argb8888 color{ 0 }; // the colour of an alpha-only surface
    };

    // At most one operation is in flight. The surfaces stay valid until onDone, which is never called from within the call.
    // The surfaces of one operation have the same size. A destination may alias the background of a blend
    class Blitter
    {
    protected:
        Blitter() = default;
        Blitter(const Blitter& other) = delete;
        Blitter& operator=(const Blitter& other) = delete;
        ~Blitter() = default;

    public:
        // A fill ignores source. A blend is asked about its foreground, and its background holds colour directly
        virtual bool Supports(BlitOperation operation, SurfaceFormat source, SurfaceFormat destination) const = 0;

        virtual void Fill(const Surface& destination, Argb8888 color, const infra::Function<void()>& onDone) = 0;

        // Converts the format of the source to the format of the destination
        virtual void Copy(const ConstSurface& source, const Surface& destination, const infra::Function<void()>& onDone) = 0;

        virtual void Blend(const BlendSource& foreground, const ConstSurface& background, const Surface& destination, const infra::Function<void()>& onDone) = 0;
    };
}

#endif
