#ifndef HAL_BLITTER_MOCK_HPP
#define HAL_BLITTER_MOCK_HPP

#include "hal/interfaces/Blitter.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class BlitterMock
        : public Blitter
    {
    public:
        MOCK_METHOD(bool, Supports, (BlitOperation operation, SurfaceFormat source, SurfaceFormat destination), (const, override));
        MOCK_METHOD(void, Fill, (const Surface& destination, Argb8888 color, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, Copy, (const ConstSurface& source, const Surface& destination, const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(void, Blend, (const BlendSource& foreground, const ConstSurface& background, const Surface& destination, const infra::Function<void()>& onDone), (override));
    };
}

#endif
