#ifndef HAL_CAMERA_HPP
#define HAL_CAMERA_HPP

#include "hal/interfaces/CameraFormat.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    class Camera
    {
    public:
        enum class Mode : uint8_t
        {
            snapshot,
            continuous
        };

        enum class Error : uint8_t
        {
            overrun,
            synchronization
        };

        using Frame = infra::ConstByteRange;

        virtual void Start(CameraFormat format, Mode mode, infra::ByteRange buffer,
            const infra::Function<void(Frame frame)>& onFrame,
            const infra::Function<void(Error error)>& onError) = 0;
        virtual void Stop() = 0;

    protected:
        Camera() = default;
        Camera(const Camera& other) = delete;
        Camera& operator=(const Camera& other) = delete;
        ~Camera() = default;
    };
}

#endif // HAL_CAMERA_HPP
