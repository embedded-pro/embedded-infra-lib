#include "hal/interfaces/CameraFormat.hpp"

namespace hal
{
    bool IsCompressed(CameraPixelFormat format)
    {
        return format == CameraPixelFormat::jpeg;
    }

    std::size_t BytesPerPixel(CameraPixelFormat format)
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

    uint64_t FrameSizeInBytes(const CameraFormat& format)
    {
        return static_cast<uint64_t>(format.width) * format.height * BytesPerPixel(format.pixelFormat);
    }

    bool IsValidFrameBuffer(const CameraFormat& format, std::size_t bufferSize)
    {
        if (format.width == 0 || format.height == 0)
            return false;

        const auto frameSize = FrameSizeInBytes(format);

        return IsCompressed(format.pixelFormat) ? bufferSize != 0 : bufferSize >= frameSize;
    }
}
