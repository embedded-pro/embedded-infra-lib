#include "drivers/display/mipi_dsi/MipiDsiVideoPanel.hpp"
#include "drivers/display/mipi_dsi/test/MipiDsiTestPanel.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using dsitest::ExpectDcs;
    using dsitest::InitializationResult;

    class MipiDsiVideoPanelTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Create(hal::PixelFormat format = hal::PixelFormat::rgb565Swapped)
        {
            panel.emplace(host, stream, reset, configuration, format, [this](InitializationResult result)
                {
                    initialized.callback(result);
                });
        }

        void CreateAndInitialize(hal::PixelFormat format = hal::PixelFormat::rgb565Swapped, uint8_t pixelFormat = 0x55)
        {
            {
                testing::InSequence sequence;
                dsitest::ExpectInitializationBeforeDisplayOn(host, dsitest::InitializationOptions{ pixelFormat });
                EXPECT_CALL(stream, StartMock());
                ExpectDcs(host, 0x29, {});
                EXPECT_CALL(initialized, callback(InitializationResult::success));
            }

            Create(format);
            ForwardTime(std::chrono::milliseconds(300));
            VerifyAll();
        }

        void CreateInitializeAndSleep()
        {
            CreateAndInitialize();
            {
                testing::InSequence sequence;
                ExpectDcs(host, 0x28, {});
                EXPECT_CALL(stream, StopMock());
                ExpectDcs(host, 0x10, {});
                EXPECT_CALL(done, callback());
            }

            panel->Sleep(onDone);
            ForwardTime(std::chrono::milliseconds(200));
            VerifyAll();
        }

        void VerifyAll()
        {
            testing::Mock::VerifyAndClearExpectations(&host);
            testing::Mock::VerifyAndClearExpectations(&stream);
            testing::Mock::VerifyAndClearExpectations(&initialized);
            testing::Mock::VerifyAndClearExpectations(&done);
        }

        testing::StrictMock<hal::DsiHostMock> host;
        testing::StrictMock<hal::DsiVideoStreamMock> stream;
        hal::GpioPinSpy reset;
        testing::StrictMock<infra::MockCallback<void(InitializationResult)>> initialized;
        testing::StrictMock<infra::MockCallback<void()>> done;
        dsitest::Panel configuration{ dsitest::MakePanel() };
        std::optional<drivers::MipiDsiVideoPanel> panel;
        infra::Function<void()> onDone{ [this]()
            {
                done.callback();
            } };
    };
}

TEST_F(MipiDsiVideoPanelTest, the_stream_is_started_after_the_after_sleep_out_table_and_before_display_on)
{
    testing::InSequence sequence;
    dsitest::ExpectInitializationBeforeDisplayOn(host);
    EXPECT_CALL(stream, StartMock());
    ExpectDcs(host, 0x29, {});
    EXPECT_CALL(initialized, callback(InitializationResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(300));
}

TEST_F(MipiDsiVideoPanelTest, display_on_waits_for_the_stream_start_to_complete)
{
    stream.completeAutomatically = false;
    {
        testing::InSequence sequence;
        dsitest::ExpectInitializationBeforeDisplayOn(host);
        EXPECT_CALL(stream, StartMock());
    }
    Create();
    ForwardTime(std::chrono::milliseconds(300));
    VerifyAll();

    ExpectDcs(host, 0x29, {});
    EXPECT_CALL(initialized, callback(InitializationResult::success));
    stream.CompletePending();
    ExecuteAllActions();
}

TEST_F(MipiDsiVideoPanelTest, a_failed_identification_never_starts_the_stream)
{
    configuration = dsitest::MakeIdentifiedPanel();
    host.readResult = hal::DsiHost::Result::failed;
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{}));
    EXPECT_CALL(initialized, callback(InitializationResult::noResponse));

    Create();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiVideoPanelTest, native_rgb565_is_accepted_as_16_bits_per_pixel)
{
    CreateAndInitialize(hal::PixelFormat::rgb565, 0x55);
}

TEST_F(MipiDsiVideoPanelTest, rgb888_selects_24_bits_per_pixel)
{
    CreateAndInitialize(hal::PixelFormat::rgb888, 0x77);
}

TEST_F(MipiDsiVideoPanelTest, a_grey_format_asserts)
{
    EXPECT_DEATH(Create(hal::PixelFormat::grey8), "");
}

TEST_F(MipiDsiVideoPanelTest, sleep_stops_the_stream_between_display_off_and_sleep_in)
{
    CreateAndInitialize();
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x28, {});
        EXPECT_CALL(stream, StopMock());
        ExpectDcs(host, 0x10, {});
        EXPECT_CALL(done, callback());
    }

    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(200));
}

TEST_F(MipiDsiVideoPanelTest, sleep_in_waits_for_the_stream_stop_to_complete)
{
    CreateAndInitialize();
    stream.completeAutomatically = false;
    ExpectDcs(host, 0x28, {});
    EXPECT_CALL(stream, StopMock());

    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(200));
    VerifyAll();

    ExpectDcs(host, 0x10, {});
    stream.CompletePending();
    ForwardTime(std::chrono::milliseconds(119));
    VerifyAll();

    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiVideoPanelTest, wake_starts_the_stream_between_sleep_out_and_display_on)
{
    CreateInitializeAndSleep();
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x11, {});
        EXPECT_CALL(stream, StartMock());
        ExpectDcs(host, 0x29, {});
        EXPECT_CALL(done, callback());
    }

    panel->Wake(onDone);
    ForwardTime(std::chrono::milliseconds(200));
}

TEST_F(MipiDsiVideoPanelTest, brightness_is_written_while_the_stream_runs)
{
    CreateAndInitialize();
    ExpectDcs(host, 0x51, { 0x80 });
    EXPECT_CALL(done, callback());

    panel->SetBrightness(0x80, onDone);
    ExecuteAllActions();
}
