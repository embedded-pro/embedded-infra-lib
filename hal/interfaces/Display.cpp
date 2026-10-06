#include "hal/interfaces/Display.hpp"

namespace hal
{
    bool IsValidDisplayWrite(DisplaySize size, PixelFormat format, const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes)
    {
        if (uint32_t{ area.x } + area.width > size.width || uint32_t{ area.y } + area.height > size.height)
        {
            return false;
        }

        if (area.width == 0 || area.height == 0)
        {
            return true;
        }

        std::size_t rowBytes = area.width * BytesPerPixel(format);
        return strideInBytes >= rowBytes && pixels.size() >= (area.height - 1) * strideInBytes + rowBytes;
    }

    void Display::Write(const DisplayArea& area, infra::ConstByteRange pixels, const infra::Function<void()>& onDone)
    {
        WriteWithStride(area, pixels, area.width * BytesPerPixel(Format()), onDone);
    }
}
