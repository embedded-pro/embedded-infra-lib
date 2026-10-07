#include "hal/interfaces/CameraFormat.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>

namespace hal
{
    bool CameraFormat::operator==(const CameraFormat& other) const
    {
        return width == other.width && height == other.height && pixelFormat == other.pixelFormat;
    }

    bool IsCompressed(CameraPixelFormat format)
    {
        return format == CameraPixelFormat::jpeg;
    }

    std::size_t BytesPerPixel(CameraPixelFormat format)
    {
        static constexpr std::array<std::size_t, 6> bytesPerPixel{ 1, 2, 2, 2, 2, 0 };
        static_assert(static_cast<std::size_t>(CameraPixelFormat::grey8) == 0);
        static_assert(static_cast<std::size_t>(CameraPixelFormat::jpeg) == bytesPerPixel.size() - 1);

        const auto index = static_cast<std::size_t>(format);
        really_assert(index < bytesPerPixel.size());

        return bytesPerPixel[index];
    }

    uint64_t FrameSizeInBytes(const CameraFormat& format)
    {
        return static_cast<uint64_t>(format.width) * format.height * BytesPerPixel(format.pixelFormat);
    }

    bool IsValidFrameBuffer(const CameraFormat& format, std::size_t bufferSize)
    {
        const bool hasArea = static_cast<uint64_t>(format.width) * format.height != 0;
        const uint64_t minimumSize = FrameSizeInBytes(format) + static_cast<uint64_t>(IsCompressed(format.pixelFormat));

        const bool bufferFits = bufferSize >= minimumSize;

        return hasArea & bufferFits;
    }
}
