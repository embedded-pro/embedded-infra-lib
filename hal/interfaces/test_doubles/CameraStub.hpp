#ifndef HAL_CAMERA_STUB_HPP
#define HAL_CAMERA_STUB_HPP

#include "hal/interfaces/test_doubles/CameraMock.hpp"

namespace hal
{
    class CameraStub
        : public CameraMock
    {
    public:
        CameraStub();

        void FrameCaptured(infra::ConstByteRange data);
        void Overrun();
        void SynchronizationLost();

        bool Running() const;
        infra::ByteRange Buffer() const;
        CameraFormat RequestedFormat() const;
        Camera::Mode RequestedMode() const;

    private:
        void FireError(Error error);

        bool running{ false };
        CameraFormat requestedFormat{};
        Camera::Mode requestedMode{ Camera::Mode::snapshot };
        infra::ByteRange buffer{};
        infra::Function<void(Frame)> onFrame;
        infra::Function<void(Error)> onError;
    };
}

#endif // HAL_CAMERA_STUB_HPP
