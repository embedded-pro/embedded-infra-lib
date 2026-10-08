#ifndef HAL_CAMERA_MOCK_HPP
#define HAL_CAMERA_MOCK_HPP

#include "hal/interfaces/Camera.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class CameraMock
        : public Camera
    {
    public:
        MOCK_METHOD(void, Start, (CameraFormat format, Mode mode, infra::ByteRange buffer, const infra::Function<void(Frame frame)>& onFrame, const infra::Function<void(Error error)>& onError), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_CAMERA_MOCK_HPP
