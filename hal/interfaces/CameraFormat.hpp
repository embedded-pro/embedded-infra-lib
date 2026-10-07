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

        bool operator==(const CameraFormat& other) const = default;
    };

    constexpr bool IsCompressed(CameraPixelFormat format)
    {
        return format == CameraPixelFormat::jpeg;
    }

    constexpr std::size_t BytesPerPixel(CameraPixelFormat format)
    {
        switch (format)
        {
            case CameraPixelFormat::grey8:
                return 1;
            case CameraPixelFormat::rgb565:
            case CameraPixelFormat::rgb565Swapped:
            case CameraPixelFormat::yuv422Yuyv:
            case CameraPixelFormat::yuv422Uyvy:
                return 2;
            case CameraPixelFormat::jpeg:
                return 0;
        }

        return 0;
    }

    constexpr std::size_t FrameSizeInBytes(const CameraFormat& format)
    {
        if (IsCompressed(format.pixelFormat))
            return 0;
        return static_cast<std::size_t>(static_cast<uint64_t>(format.width) * format.height * BytesPerPixel(format.pixelFormat));
    }

    constexpr bool IsValidFrameBuffer(const CameraFormat& format, std::size_t bufferSize)
    {
        if (format.width == 0 || format.height == 0)
            return false;
        if (IsCompressed(format.pixelFormat))
            return bufferSize != 0;
        return bufferSize >= static_cast<uint64_t>(format.width) * format.height * BytesPerPixel(format.pixelFormat);
    }
}

#endif // HAL_CAMERA_FORMAT_HPP
