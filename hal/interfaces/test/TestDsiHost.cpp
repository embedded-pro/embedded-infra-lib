#include "hal/interfaces/test_doubles/DsiHostStub.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    constexpr hal::DisplaySize screenSize{ 4, 3 };
    constexpr std::size_t bytesPerPixel = 2;
    constexpr std::size_t framebufferSize = 4 * 3 * bytesPerPixel;

    constexpr uint8_t setColumnAddress = 0x2a;
    constexpr uint8_t setPageAddress = 0x2b;
    constexpr uint8_t writeMemoryStart = 0x2c;
    constexpr uint8_t writeMemoryContinue = 0x3c;

    std::array<uint8_t, 2> Pixel(uint8_t first, uint8_t second)
    {
        return { first, second };
    }

    class DsiHostMockTest
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        testing::StrictMock<hal::DsiHostMock> host;
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
        std::optional<hal::DsiHost::Result> readCompletion;
        infra::Function<void(hal::DsiHost::Result)> onRead{ [this](hal::DsiHost::Result result)
            {
                readCompletion = result;
            } };
    };

    class DsiVideoStreamMockTest
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        testing::StrictMock<hal::DsiVideoStreamMock> stream;
        int completions{ 0 };
        infra::Function<void()> onDone{ [this]()
            {
                ++completions;
            } };
    };

    class DsiHostStubTest
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        DsiHostStubTest()
        {
            EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(testing::AnyNumber());
        }

        void Send(uint8_t command, const std::vector<uint8_t>& parameters)
        {
            host.WriteDcs(command, infra::MakeRange(parameters), []() {});
            ExecuteAllActions();
        }

        void SetWindow(uint16_t firstColumn, uint16_t lastColumn, uint16_t firstPage, uint16_t lastPage)
        {
            Send(setColumnAddress, { static_cast<uint8_t>(firstColumn >> 8), static_cast<uint8_t>(firstColumn), static_cast<uint8_t>(lastColumn >> 8), static_cast<uint8_t>(lastColumn) });
            Send(setPageAddress, { static_cast<uint8_t>(firstPage >> 8), static_cast<uint8_t>(firstPage), static_cast<uint8_t>(lastPage >> 8), static_cast<uint8_t>(lastPage) });
        }

        void ExpectPixel(uint16_t x, uint16_t y, std::array<uint8_t, 2> expected)
        {
            infra::ConstByteRange pixel = host.PixelAt(x, y);
            EXPECT_EQ(expected, (std::array<uint8_t, 2>{ pixel[0], pixel[1] }));
        }

        testing::StrictMock<hal::DsiHostStub::WithStorage<framebufferSize>> host{ screenSize, hal::PixelFormat::rgb565Swapped };
    };
}

TEST_F(DsiHostMockTest, max_parameters_size_is_reported)
{
    host.maxParametersSize = 16;

    EXPECT_EQ(std::size_t(16), host.MaxParametersSize());
}

TEST_F(DsiHostMockTest, a_dcs_write_is_forwarded_with_its_command_and_parameters)
{
    std::array<uint8_t, 3> parameters{ 1, 2, 3 };
    EXPECT_CALL(host, WriteDcsMock(0x2a, std::vector<uint8_t>{ 1, 2, 3 }));

    host.WriteDcs(0x2a, parameters, onDone);
    ExecuteAllActions();
}

TEST_F(DsiHostMockTest, a_generic_write_is_forwarded_with_its_data)
{
    std::array<uint8_t, 2> data{ 0xb9, 0x01 };
    EXPECT_CALL(host, WriteGenericMock(std::vector<uint8_t>{ 0xb9, 0x01 }));

    host.WriteGeneric(data, onDone);
    ExecuteAllActions();
}

TEST_F(DsiHostMockTest, completion_is_delivered_on_the_dispatcher_and_not_within_the_call)
{
    EXPECT_CALL(host, WriteDcsMock(0x29, std::vector<uint8_t>{}));

    host.WriteDcs(0x29, infra::ConstByteRange(), onDone);
    EXPECT_EQ(0, completions);
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(DsiHostMockTest, completion_is_held_until_complete_pending_when_not_automatic)
{
    host.completeAutomatically = false;
    EXPECT_CALL(host, WriteDcsMock(0x29, std::vector<uint8_t>{}));

    host.WriteDcs(0x29, infra::ConstByteRange(), onDone);
    ExecuteAllActions();

    EXPECT_TRUE(host.CompletionPending());
    EXPECT_EQ(0, completions);

    host.CompletePending();

    EXPECT_FALSE(host.CompletionPending());
    EXPECT_EQ(1, completions);
}

TEST_F(DsiHostMockTest, a_second_operation_while_one_is_pending_asserts)
{
    host.completeAutomatically = false;
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    host.WriteDcs(0x29, infra::ConstByteRange(), onDone);

    EXPECT_DEATH(host.WriteDcs(0x28, infra::ConstByteRange(), onDone), "");
}

TEST_F(DsiHostMockTest, an_operation_can_be_started_from_the_completion)
{
    EXPECT_CALL(host, WriteDcsMock(0x28, std::vector<uint8_t>{}));
    EXPECT_CALL(host, WriteDcsMock(0x29, std::vector<uint8_t>{}));

    host.WriteDcs(0x28, infra::ConstByteRange(), [this]()
        {
            host.WriteDcs(0x29, infra::ConstByteRange(), onDone);
        });
    ExecuteAllActions();

    EXPECT_EQ(1, completions);
}

TEST_F(DsiHostMockTest, dcs_parameters_larger_than_the_maximum_assert)
{
    host.maxParametersSize = 2;
    std::array<uint8_t, 3> parameters{ 1, 2, 3 };

    EXPECT_DEATH(host.WriteDcs(0x2a, parameters, onDone), "");
}

TEST_F(DsiHostMockTest, generic_data_larger_than_the_maximum_assert)
{
    host.maxParametersSize = 2;
    std::array<uint8_t, 3> data{ 1, 2, 3 };

    EXPECT_DEATH(host.WriteGeneric(data, onDone), "");
}

TEST_F(DsiHostMockTest, a_read_fills_the_buffer_and_reports_success)
{
    std::array<uint8_t, 3> buffer{};
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3 }));

    host.ReadDcs(0x04, buffer, onRead);
    EXPECT_FALSE(readCompletion);
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 3>{ 1, 2, 3 }), buffer);
    ASSERT_TRUE(readCompletion);
    EXPECT_EQ(hal::DsiHost::Result::success, *readCompletion);
}

TEST_F(DsiHostMockTest, a_failed_read_leaves_the_buffer_untouched_and_reports_the_result)
{
    std::array<uint8_t, 3> buffer{ 9, 9, 9 };
    host.readResult = hal::DsiHost::Result::timeout;
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{}));

    host.ReadDcs(0x04, buffer, onRead);
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 3>{ 9, 9, 9 }), buffer);
    ASSERT_TRUE(readCompletion);
    EXPECT_EQ(hal::DsiHost::Result::timeout, *readCompletion);
}

TEST_F(DsiHostMockTest, a_read_completion_is_held_until_complete_pending_when_not_automatic)
{
    std::array<uint8_t, 1> buffer{};
    host.completeAutomatically = false;
    EXPECT_CALL(host, ReadDcsMock(0x0a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x9c }));

    host.ReadDcs(0x0a, buffer, onRead);
    ExecuteAllActions();
    EXPECT_FALSE(readCompletion);
    EXPECT_TRUE(host.CompletionPending());

    host.CompletePending();

    ASSERT_TRUE(readCompletion);
    EXPECT_EQ(hal::DsiHost::Result::success, *readCompletion);
}

TEST_F(DsiVideoStreamMockTest, start_and_stop_complete_on_the_dispatcher)
{
    EXPECT_CALL(stream, StartMock());
    EXPECT_CALL(stream, StopMock());

    stream.Start(onDone);
    EXPECT_EQ(0, completions);
    ExecuteAllActions();
    EXPECT_EQ(1, completions);

    stream.Stop(onDone);
    EXPECT_EQ(1, completions);
    ExecuteAllActions();
    EXPECT_EQ(2, completions);
}

TEST_F(DsiVideoStreamMockTest, completion_is_held_until_complete_pending_when_not_automatic)
{
    stream.completeAutomatically = false;
    EXPECT_CALL(stream, StartMock());

    stream.Start(onDone);
    ExecuteAllActions();

    EXPECT_TRUE(stream.CompletionPending());
    EXPECT_EQ(0, completions);

    stream.CompletePending();

    EXPECT_FALSE(stream.CompletionPending());
    EXPECT_EQ(1, completions);
}

TEST_F(DsiVideoStreamMockTest, an_operation_while_another_is_pending_asserts)
{
    stream.completeAutomatically = false;
    EXPECT_CALL(stream, StartMock());
    stream.Start(onDone);

    EXPECT_DEATH(stream.Stop(onDone), "");
}

TEST_F(DsiHostStubTest, pixels_are_stored_at_the_addressed_window)
{
    SetWindow(1, 2, 1, 1);

    Send(writeMemoryStart, { 1, 2, 3, 4 });

    ExpectPixel(1, 1, Pixel(1, 2));
    ExpectPixel(2, 1, Pixel(3, 4));
    ExpectPixel(0, 0, Pixel(0, 0));
    ExpectPixel(3, 1, Pixel(0, 0));
}

TEST_F(DsiHostStubTest, writing_wraps_to_the_next_row_at_the_end_of_the_column_range)
{
    SetWindow(1, 2, 0, 1);

    Send(writeMemoryStart, { 1, 1, 2, 2, 3, 3, 4, 4 });

    ExpectPixel(1, 0, Pixel(1, 1));
    ExpectPixel(2, 0, Pixel(2, 2));
    ExpectPixel(1, 1, Pixel(3, 3));
    ExpectPixel(2, 1, Pixel(4, 4));
}

TEST_F(DsiHostStubTest, write_memory_continue_resumes_after_the_previous_chunk)
{
    SetWindow(0, 3, 0, 0);

    Send(writeMemoryStart, { 1, 1, 2, 2 });
    Send(writeMemoryContinue, { 3, 3, 4, 4 });

    ExpectPixel(0, 0, Pixel(1, 1));
    ExpectPixel(1, 0, Pixel(2, 2));
    ExpectPixel(2, 0, Pixel(3, 3));
    ExpectPixel(3, 0, Pixel(4, 4));
}

TEST_F(DsiHostStubTest, write_memory_start_restarts_at_the_window_origin)
{
    SetWindow(0, 3, 0, 0);

    Send(writeMemoryStart, { 1, 1, 2, 2 });
    Send(writeMemoryStart, { 5, 5 });

    ExpectPixel(0, 0, Pixel(5, 5));
    ExpectPixel(1, 0, Pixel(2, 2));
}

TEST_F(DsiHostStubTest, other_commands_do_not_change_the_frame_memory)
{
    Send(0x29, {});
    Send(0x3a, { 0x55 });
    Send(0x36, { 0x48 });

    ExpectPixel(0, 0, Pixel(0, 0));
}

TEST_F(DsiHostStubTest, a_chunk_that_splits_a_pixel_asserts)
{
    SetWindow(0, 3, 0, 0);

    EXPECT_DEATH(Send(writeMemoryStart, { 1, 1, 2 }), "");
}

TEST_F(DsiHostStubTest, pixels_beyond_the_window_assert)
{
    SetWindow(0, 0, 0, 0);

    EXPECT_DEATH(Send(writeMemoryStart, { 1, 1, 2, 2 }), "");
}

TEST_F(DsiHostStubTest, write_memory_continue_before_write_memory_start_asserts)
{
    EXPECT_DEATH(Send(writeMemoryContinue, { 1, 1 }), "");
}

TEST_F(DsiHostStubTest, a_window_outside_the_display_asserts)
{
    EXPECT_DEATH(Send(setColumnAddress, { 0, 0, 0, 4 }), "");
    EXPECT_DEATH(Send(setPageAddress, { 0, 0, 0, 3 }), "");
}

TEST_F(DsiHostStubTest, a_window_with_the_end_before_the_start_asserts)
{
    EXPECT_DEATH(Send(setColumnAddress, { 0, 2, 0, 1 }), "");
}

TEST_F(DsiHostStubTest, a_window_command_without_four_parameters_asserts)
{
    EXPECT_DEATH(Send(setColumnAddress, { 0, 0, 0 }), "");
}
