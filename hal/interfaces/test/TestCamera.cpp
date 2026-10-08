#include "hal/interfaces/test_doubles/CameraStub.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr hal::CameraFormat testFormat{ 2, 2, hal::CameraPixelFormat::grey8 };

    class CameraTest
        : public testing::Test
    {
    public:
        void StartSnapshot(hal::CameraFormat format = testFormat)
        {
            EXPECT_CALL(stub, Start(format, hal::Camera::Mode::snapshot, testing::_, testing::_, testing::_));

            camera.Start(
                format, hal::Camera::Mode::snapshot, infra::MakeRange(frameBuffer),
                [this](hal::Camera::Frame frame)
                {
                    ++framesReceived;
                    receivedFrame = frame;
                },
                [this](hal::Camera::Error error)
                {
                    ++errorsReceived;
                    lastError = error;
                });
        }

        void StartContinuous(hal::CameraFormat format = testFormat)
        {
            EXPECT_CALL(stub, Start(format, hal::Camera::Mode::continuous, testing::_, testing::_, testing::_));

            camera.Start(
                format, hal::Camera::Mode::continuous, infra::MakeRange(frameBuffer),
                [this](hal::Camera::Frame frame)
                {
                    ++framesReceived;
                    receivedFrame = frame;
                },
                [this](hal::Camera::Error error)
                {
                    ++errorsReceived;
                    lastError = error;
                });
        }

        testing::StrictMock<hal::CameraStub> stub;
        hal::Camera& camera{ stub };
        std::array<uint8_t, 16> frameBuffer{};
        int framesReceived{ 0 };
        hal::Camera::Frame receivedFrame{};
        int errorsReceived{ 0 };
        hal::Camera::Error lastError{ hal::Camera::Error::overrun };
    };
}

TEST_F(CameraTest, Start_hands_format_mode_and_buffer_to_the_implementation)
{
    StartSnapshot(testFormat);

    EXPECT_EQ(testFormat, stub.RequestedFormat());
    EXPECT_EQ(hal::Camera::Mode::snapshot, stub.RequestedMode());
    EXPECT_EQ(frameBuffer.data(), stub.Buffer().begin());
}

TEST_F(CameraTest, Stop_halts_the_capture)
{
    EXPECT_CALL(stub, Stop());

    camera.Stop();
}

TEST_F(CameraTest, no_frame_is_received_before_one_is_captured)
{
    StartSnapshot();

    EXPECT_EQ(0, framesReceived);
}

TEST_F(CameraTest, captured_frame_reaches_the_consumer_as_a_range_of_the_buffer)
{
    StartSnapshot();

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
    EXPECT_EQ(std::size_t(4), receivedFrame.size());
    EXPECT_EQ(frameBuffer.data(), receivedFrame.begin());
    EXPECT_EQ(uint8_t(1), receivedFrame[0]);
    EXPECT_EQ(uint8_t(4), receivedFrame[3]);
}

TEST_F(CameraTest, captured_frame_may_be_shorter_than_the_buffer)
{
    StartSnapshot();

    std::array<uint8_t, 2> captured{ 10, 20 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
    EXPECT_EQ(std::size_t(2), receivedFrame.size());
}

TEST_F(CameraTest, snapshot_delivers_one_frame_and_the_camera_is_stopped_again)
{
    StartSnapshot();

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
    EXPECT_FALSE(stub.Running());

    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
}

TEST_F(CameraTest, snapshot_may_be_restarted_from_within_the_frame_callback)
{
    int secondFrameReceived{ 0 };
    EXPECT_CALL(stub, Start(testFormat, hal::Camera::Mode::snapshot, testing::_, testing::_, testing::_)).Times(2);

    camera.Start(
        testFormat, hal::Camera::Mode::snapshot, infra::MakeRange(frameBuffer),
        [&](hal::Camera::Frame)
        {
            ++framesReceived;
            if (framesReceived == 1)
                camera.Start(
                    testFormat, hal::Camera::Mode::snapshot, infra::MakeRange(frameBuffer),
                    [&](hal::Camera::Frame)
                    {
                        ++secondFrameReceived;
                    },
                    [](hal::Camera::Error) {});
        },
        [](hal::Camera::Error) {});

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
    EXPECT_EQ(1, secondFrameReceived);
}

TEST_F(CameraTest, continuous_keeps_delivering_frames)
{
    StartContinuous();

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));
    stub.FrameCaptured(infra::MakeRange(captured));
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(3, framesReceived);
    EXPECT_TRUE(stub.Running());
}

TEST_F(CameraTest, overrun_is_reported_through_the_error_callback)
{
    StartSnapshot();

    stub.Overrun();

    EXPECT_EQ(1, errorsReceived);
    EXPECT_EQ(hal::Camera::Error::overrun, lastError);
    EXPECT_EQ(0, framesReceived);
}

TEST_F(CameraTest, synchronization_loss_is_reported_through_the_error_callback)
{
    StartSnapshot();

    stub.SynchronizationLost();

    EXPECT_EQ(1, errorsReceived);
    EXPECT_EQ(hal::Camera::Error::synchronization, lastError);
}

TEST_F(CameraTest, snapshot_is_over_after_an_error)
{
    StartSnapshot();

    stub.Overrun();

    EXPECT_FALSE(stub.Running());
}

TEST_F(CameraTest, continuous_keeps_running_after_an_error)
{
    StartContinuous();

    stub.Overrun();

    EXPECT_TRUE(stub.Running());

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
}

TEST_F(CameraTest, no_frame_is_received_after_Stop)
{
    StartContinuous();
    EXPECT_CALL(stub, Stop());
    camera.Stop();

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(0, framesReceived);
}

TEST_F(CameraTest, no_error_is_reported_after_Stop)
{
    StartContinuous();
    EXPECT_CALL(stub, Stop());
    camera.Stop();

    stub.Overrun();

    EXPECT_EQ(0, errorsReceived);
}

TEST_F(CameraTest, consumer_may_stop_from_within_a_frame)
{
    EXPECT_CALL(stub, Start(testFormat, hal::Camera::Mode::continuous, testing::_, testing::_, testing::_));
    EXPECT_CALL(stub, Stop());

    camera.Start(
        testFormat, hal::Camera::Mode::continuous, infra::MakeRange(frameBuffer),
        [&](hal::Camera::Frame)
        {
            ++framesReceived;
            camera.Stop();
        },
        [](hal::Camera::Error) {});

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, framesReceived);
}

TEST_F(CameraTest, restarting_registers_new_callbacks)
{
    StartSnapshot();
    EXPECT_CALL(stub, Stop());
    camera.Stop();

    int restartedFrames{ 0 };
    EXPECT_CALL(stub, Start(testFormat, hal::Camera::Mode::continuous, testing::_, testing::_, testing::_));
    camera.Start(
        testFormat, hal::Camera::Mode::continuous, infra::MakeRange(frameBuffer),
        [&](hal::Camera::Frame)
        {
            ++restartedFrames;
        },
        [](hal::Camera::Error) {});

    std::array<uint8_t, 4> captured{ 1, 2, 3, 4 };
    stub.FrameCaptured(infra::MakeRange(captured));

    EXPECT_EQ(1, restartedFrames);
    EXPECT_EQ(0, framesReceived);
}
