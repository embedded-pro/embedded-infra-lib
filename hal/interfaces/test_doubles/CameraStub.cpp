#include "hal/interfaces/test_doubles/CameraStub.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    CameraStub::CameraStub()
    {
        ON_CALL(*this, Start).WillByDefault([this](CameraFormat format, Mode mode, infra::ByteRange buf, const infra::Function<void(Frame)>& onFrameCallback, const infra::Function<void(Error)>& onErrorCallback)
            {
                really_assert(!running);
                running = true;
                requestedFormat = format;
                requestedMode = mode;
                buffer = buf;
                onFrame = onFrameCallback;
                onError = onErrorCallback;
            });

        ON_CALL(*this, Stop).WillByDefault([this]()
            {
                running = false;
                onFrame = nullptr;
                onError = nullptr;
            });
    }

    void CameraStub::FrameCaptured(infra::ConstByteRange data)
    {
        if (!running)
            return;
        really_assert(data.size() <= buffer.size());

        std::copy(data.begin(), data.end(), buffer.begin());
        Frame frame{ buffer.begin(), buffer.begin() + data.size() };

        if (requestedMode == Mode::snapshot)
        {
            running = false;
            auto localOnFrame = onFrame;
            onFrame = nullptr;
            onError = nullptr;
            localOnFrame(frame);
        }
        else
        {
            auto localOnFrame = onFrame;
            localOnFrame(frame);
        }
    }

    void CameraStub::Overrun()
    {
        FireError(Error::overrun);
    }

    void CameraStub::SynchronizationLost()
    {
        FireError(Error::synchronization);
    }

    bool CameraStub::Running() const
    {
        return running;
    }

    infra::ByteRange CameraStub::Buffer() const
    {
        return buffer;
    }

    CameraFormat CameraStub::RequestedFormat() const
    {
        return requestedFormat;
    }

    Camera::Mode CameraStub::RequestedMode() const
    {
        return requestedMode;
    }

    void CameraStub::FireError(Error error)
    {
        if (!onError)
            return;

        if (requestedMode == Mode::snapshot)
        {
            running = false;
            auto localOnError = onError;
            onFrame = nullptr;
            onError = nullptr;
            localOnError(error);
        }
        else
        {
            auto localOnError = onError;
            localOnError(error);
        }
    }
}
