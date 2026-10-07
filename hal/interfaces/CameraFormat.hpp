#ifndef HAL_CAMERA_FORMAT_HPP
#define HAL_CAMERA_FORMAT_HPP

#include <cstddef>
#include <cstdint>

namespace hal
{
    enum class CameraPixelFormat : uint8_t
    {
        grey8,
        rgb565,
        rgb565Swapped,
        yuv422Yuyv,
        yuv422Uyvy,
        jpeg
    };

    struct CameraFormat
    {
        uint16_t width;
        uint16_t height;
        CameraPixelFormat pixelFormat;

        bool operator==(const CameraFormat& other) const;
    };

    bool IsCompressed(CameraPixelFormat format);
    std::size_t BytesPerPixel(CameraPixelFormat format);
    uint64_t FrameSizeInBytes(const CameraFormat& format);
    bool IsValidFrameBuffer(const CameraFormat& format, std::size_t bufferSize);
}

#endif // HAL_CAMERA_FORMAT_HPP
