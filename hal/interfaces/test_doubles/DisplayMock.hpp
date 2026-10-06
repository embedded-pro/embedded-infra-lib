#ifndef HAL_DISPLAY_MOCK_HPP
#define HAL_DISPLAY_MOCK_HPP

#include "hal/interfaces/Display.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class DisplayMock
        : public Display
    {
    public:
        MOCK_METHOD(DisplaySize, Size, (), (const, override));
        MOCK_METHOD(PixelFormat, Format, (), (const, override));
        MOCK_METHOD(void, WriteWithStride, (const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone), (override));
    };
}

#endif
