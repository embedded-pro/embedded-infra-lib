#ifndef HAL_DISPLAY_HPP
#define HAL_DISPLAY_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    struct DisplaySize
    {
        uint16_t width;
        uint16_t height;

        bool operator==(const DisplaySize& other) const = default;
    };

    struct DisplayArea
    {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;

        bool operator==(const DisplayArea& other) const = default;
    };

    // Describes the layout of the pixel bytes a display expects. rgb565 is a native-endian 16-bit word,
    // rgb565Swapped holds the same value with the high byte first. rgb888 is three bytes per pixel.
    enum class PixelFormat : uint8_t
    {
        grey8,
        rgb565,
        rgb565Swapped,
        rgb888
    };

    constexpr std::size_t BytesPerPixel(PixelFormat format)
    {
        switch (format)
        {
            case PixelFormat::grey8:
                return 1;
            case PixelFormat::rgb565:
            case PixelFormat::rgb565Swapped:
                return 2;
            case PixelFormat::rgb888:
                return 3;
        }

        return 0;
    }

    bool IsValidDisplayWrite(DisplaySize size, PixelFormat format, const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes);

    class Display
    {
    protected:
        Display() = default;
        Display(const Display& other) = delete;
        Display& operator=(const Display& other) = delete;
        ~Display() = default;

    public:
        virtual DisplaySize Size() const = 0;
        virtual PixelFormat Format() const = 0;

        // At most one write is in flight. The pixels stay valid until onDone, which is never called from within
        // the call that started the write. strideInBytes is the distance between the starts of two rows in pixels.
        virtual void WriteWithStride(const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone) = 0;

        void Write(const DisplayArea& area, infra::ConstByteRange pixels, const infra::Function<void()>& onDone);
    };
}

#endif
