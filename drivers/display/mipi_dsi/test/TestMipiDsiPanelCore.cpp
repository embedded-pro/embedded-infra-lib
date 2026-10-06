#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "drivers/display/mipi_dsi/test/MipiDsiTestPanel.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using dsitest::Command;
    using dsitest::ExpectDcs;
    using dsitest::InitializationOptions;
    using dsitest::InitializationResult;
    using dsitest::Packet;

    constexpr std::array<uint8_t, 1> tearingEffectParameter{ 0x00 };
    constexpr std::array<Command, 1> tearingEffectOn{ { { Packet::dcs, 0x35, tearingEffectParameter, 0 } } };
    constexpr std::array<uint8_t, 9> oversizedIdentification{};

    struct Hooks
    {
        testing::StrictMock<infra::MockCallback<void()>> beforeDisplayOn;
        testing::StrictMock<infra::MockCallback<void()>> afterDisplayOff;
        bool hold{ false };
        infra::AutoResetFunction<void()> held;
    };

    class TestPanel
        : public drivers::MipiDsiPanelCore
    {
    public:
        TestPanel(Hooks& hooks, hal::DsiHost& host, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, infra::MemoryRange<const Command> extraCommands, const infra::Function<void(InitializationResult)>& onInitialized)
            : MipiDsiPanelCore(host, reset, panel, format, extraCommands)
            , hooks(hooks)
        {
            StartInitialization(onInitialized);
        }

    private:
        void BeforeDisplayOn(const infra::Function<void()>& onDone) override
        {
            Run(hooks.beforeDisplayOn, onDone);
        }

        void AfterDisplayOff(const infra::Function<void()>& onDone) override
        {
            Run(hooks.afterDisplayOff, onDone);
        }

        void Run(const infra::MockCallback<void()>& hook, const infra::Function<void()>& onDone)
        {
            hook.callback();

            if (hooks.hold)
                hooks.held = onDone;
            else
                onDone();
        }

    private:
        Hooks& hooks;
    };

    class MipiDsiPanelCoreTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Create(hal::PixelFormat format = hal::PixelFormat::rgb565Swapped)
        {
            CreateWith(reset, format, infra::MemoryRange<const Command>());
        }

        void CreateWith(hal::GpioPin& resetPin, hal::PixelFormat format, infra::MemoryRange<const Command> extraCommands)
        {
            panel.emplace(hooks, host, resetPin, configuration, format, extraCommands, [this](InitializationResult result)
                {
                    initialized.callback(result);
                });
        }

        void InitializeWith(hal::PixelFormat format, InitializationOptions options, infra::MemoryRange<const Command> extraCommands = infra::MemoryRange<const Command>())
        {
            {
                testing::InSequence sequence;
                dsitest::ExpectInitializationBeforeDisplayOn(host, options);
                EXPECT_CALL(hooks.beforeDisplayOn, callback());
                ExpectDcs(host, 0x29, {});
                EXPECT_CALL(initialized, callback(InitializationResult::success));
            }

            CreateWith(reset, format, extraCommands);
            ForwardTime(std::chrono::milliseconds(300));
        }

        void CreateAndInitialize()
        {
            InitializeWith(hal::PixelFormat::rgb565Swapped, InitializationOptions());
            VerifyAll();
        }

        void CreateInitializeAndSleep()
        {
            CreateAndInitialize();
            {
                testing::InSequence sequence;
                ExpectDcs(host, 0x28, {});
                EXPECT_CALL(hooks.afterDisplayOff, callback());
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
            testing::Mock::VerifyAndClearExpectations(&hooks.beforeDisplayOn);
            testing::Mock::VerifyAndClearExpectations(&hooks.afterDisplayOff);
            testing::Mock::VerifyAndClearExpectations(&initialized);
            testing::Mock::VerifyAndClearExpectations(&done);
        }

        void ExpectIdentificationRead(uint8_t command, std::vector<uint8_t> identification)
        {
            EXPECT_CALL(host, ReadDcsMock(command, identification.size())).WillOnce(testing::Return(identification));
        }

        testing::StrictMock<hal::DsiHostMock> host;
        hal::GpioPinSpy reset;
        Hooks hooks;
        testing::StrictMock<infra::MockCallback<void(InitializationResult)>> initialized;
        testing::StrictMock<infra::MockCallback<void()>> done;
        dsitest::Panel configuration{ dsitest::MakePanel() };
        std::optional<TestPanel> panel;
        infra::Function<void()> onDone{ [this]()
            {
                done.callback();
            } };
    };
}

TEST_F(MipiDsiPanelCoreTest, reset_is_held_low_for_the_reset_pulse_and_then_released)
{
    dsitest::AllowAnyCommands(host);
    EXPECT_CALL(hooks.beforeDisplayOn, callback());
    EXPECT_CALL(initialized, callback(InitializationResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(300));

    EXPECT_EQ((std::vector<hal::PinChange>{ { std::chrono::milliseconds(0), true }, { std::chrono::milliseconds(0), false }, { std::chrono::milliseconds(10), true } }), reset.PinChanges());
}

TEST_F(MipiDsiPanelCoreTest, no_command_is_sent_before_the_reset_recovery_has_elapsed)
{
    Create();
    ForwardTime(std::chrono::milliseconds(129));
    VerifyAll();

    dsitest::AllowAnyCommands(host);
    ExpectDcs(host, 0xb9, { 0xff, 0x83 });
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, custom_reset_timings_are_used)
{
    configuration.timings.resetPulse = std::chrono::milliseconds(5);
    configuration.timings.resetRecovery = std::chrono::milliseconds(20);
    Create();

    ForwardTime(std::chrono::milliseconds(24));
    VerifyAll();
    EXPECT_EQ((std::vector<hal::PinChange>{ { std::chrono::milliseconds(0), true }, { std::chrono::milliseconds(0), false }, { std::chrono::milliseconds(5), true } }), reset.PinChanges());

    dsitest::AllowAnyCommands(host);
    ExpectDcs(host, 0xb9, { 0xff, 0x83 });
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_soft_reset_replaces_the_reset_pulse_when_no_reset_pin_is_connected)
{
    ExpectDcs(host, 0x01, {});
    CreateWith(hal::dummyPin, hal::PixelFormat::rgb565Swapped, infra::MemoryRange<const Command>());

    ForwardTime(std::chrono::milliseconds(119));
    VerifyAll();

    dsitest::AllowAnyCommands(host);
    ExpectDcs(host, 0xb9, { 0xff, 0x83 });
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, initialization_writes_the_tables_around_sleep_out_in_order)
{
    testing::InSequence sequence;
    dsitest::ExpectInitializationBeforeDisplayOn(host);
    EXPECT_CALL(hooks.beforeDisplayOn, callback());
    ExpectDcs(host, 0x29, {});
    EXPECT_CALL(initialized, callback(InitializationResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(300));
}

TEST_F(MipiDsiPanelCoreTest, a_command_delay_postpones_the_next_command)
{
    ExpectDcs(host, 0xb9, { 0xff, 0x83 });
    dsitest::ExpectGeneric(host, { 0xba, 0x01 });
    Create();

    ForwardTime(std::chrono::milliseconds(139));
    VerifyAll();

    ExpectDcs(host, 0x11, {});
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, sleep_out_is_followed_by_the_sleep_out_delay)
{
    dsitest::AllowAnyCommands(host);
    Create();
    ForwardTime(std::chrono::milliseconds(259));
    VerifyAll();

    dsitest::AllowAnyCommands(host);
    ExpectDcs(host, 0x3a, { 0x55 });
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_custom_sleep_out_delay_is_used)
{
    configuration.timings.sleepOutDelay = std::chrono::milliseconds(50);
    dsitest::AllowAnyCommands(host);
    Create();
    ForwardTime(std::chrono::milliseconds(189));
    VerifyAll();

    dsitest::AllowAnyCommands(host);
    ExpectDcs(host, 0x3a, { 0x55 });
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, pixel_format_is_0x55_for_rgb565_swapped)
{
    InitializeWith(hal::PixelFormat::rgb565Swapped, InitializationOptions{ 0x55 });
}

TEST_F(MipiDsiPanelCoreTest, pixel_format_is_0x55_for_rgb565)
{
    InitializeWith(hal::PixelFormat::rgb565, InitializationOptions{ 0x55 });
}

TEST_F(MipiDsiPanelCoreTest, pixel_format_is_0x77_for_rgb888)
{
    InitializeWith(hal::PixelFormat::rgb888, InitializationOptions{ 0x77 });
}

TEST_F(MipiDsiPanelCoreTest, a_grey_format_asserts)
{
    EXPECT_DEATH(Create(hal::PixelFormat::grey8), "");
}

TEST_F(MipiDsiPanelCoreTest, the_address_mode_byte_is_taken_from_the_panel)
{
    configuration.addressMode = 0x08;

    InitializeWith(hal::PixelFormat::rgb565Swapped, InitializationOptions{ 0x55, 0x08 });
}

TEST_F(MipiDsiPanelCoreTest, extra_commands_follow_the_address_mode_and_precede_the_after_sleep_out_table)
{
    InitializeWith(hal::PixelFormat::rgb565Swapped, InitializationOptions{ 0x55, 0x48, true }, tearingEffectOn);
}

TEST_F(MipiDsiPanelCoreTest, display_on_waits_for_the_before_display_on_hook_to_complete)
{
    hooks.hold = true;
    {
        testing::InSequence sequence;
        dsitest::ExpectInitializationBeforeDisplayOn(host);
        EXPECT_CALL(hooks.beforeDisplayOn, callback());
    }
    Create();
    ForwardTime(std::chrono::milliseconds(300));
    VerifyAll();

    ExpectDcs(host, 0x29, {});
    EXPECT_CALL(initialized, callback(InitializationResult::success));
    hooks.held();
    ExecuteAllActions();
}

TEST_F(MipiDsiPanelCoreTest, initialization_is_reported_once_after_display_on_and_its_delay)
{
    configuration.timings.displayOnDelay = std::chrono::milliseconds(50);
    {
        testing::InSequence sequence;
        dsitest::ExpectInitializationBeforeDisplayOn(host);
        EXPECT_CALL(hooks.beforeDisplayOn, callback());
        ExpectDcs(host, 0x29, {});
    }
    Create();
    ForwardTime(std::chrono::milliseconds(314));
    VerifyAll();

    EXPECT_CALL(initialized, callback(InitializationResult::success));
    ForwardTime(std::chrono::milliseconds(1));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_command_larger_than_the_host_maximum_asserts)
{
    host.maxParametersSize = 1;
    auto runPastReset = [this]()
    {
        Create();
        ForwardTime(std::chrono::milliseconds(130));
    };

    EXPECT_DEATH(runPastReset(), "");
}

TEST_F(MipiDsiPanelCoreTest, the_identification_is_read_before_the_first_table_command)
{
    configuration = dsitest::MakeIdentifiedPanel();
    {
        testing::InSequence sequence;
        ExpectIdentificationRead(0x04, { 0x12, 0x34, 0x56 });
        dsitest::ExpectInitializationBeforeDisplayOn(host);
        EXPECT_CALL(hooks.beforeDisplayOn, callback());
        ExpectDcs(host, 0x29, {});
        EXPECT_CALL(initialized, callback(InitializationResult::success));
    }

    Create();
    ForwardTime(std::chrono::milliseconds(300));
}

TEST_F(MipiDsiPanelCoreTest, a_custom_identification_command_is_used)
{
    configuration = dsitest::MakePanel(dsitest::Identification{ 0xda, dsitest::panelIdentification });
    ExpectIdentificationRead(0xda, { 0x12, 0x34, 0x56 });
    dsitest::AllowAnyCommands(host);
    EXPECT_CALL(hooks.beforeDisplayOn, callback());
    EXPECT_CALL(initialized, callback(InitializationResult::success));

    Create();
    ForwardTime(std::chrono::milliseconds(300));
}

TEST_F(MipiDsiPanelCoreTest, a_mismatching_identification_reports_unexpected_id_and_sends_nothing_more)
{
    configuration = dsitest::MakeIdentifiedPanel();
    ExpectIdentificationRead(0x04, { 0x12, 0x34, 0x57 });
    EXPECT_CALL(initialized, callback(InitializationResult::unexpectedId));

    Create();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_read_timeout_reports_no_response_and_sends_nothing_more)
{
    configuration = dsitest::MakeIdentifiedPanel();
    host.readResult = hal::DsiHost::Result::timeout;
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{}));
    EXPECT_CALL(initialized, callback(InitializationResult::noResponse));

    Create();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_read_failure_reports_no_response)
{
    configuration = dsitest::MakeIdentifiedPanel();
    host.readResult = hal::DsiHost::Result::failed;
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{}));
    EXPECT_CALL(initialized, callback(InitializationResult::noResponse));

    Create();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiPanelCoreTest, operations_assert_after_a_failed_initialization)
{
    configuration = dsitest::MakeIdentifiedPanel();
    host.readResult = hal::DsiHost::Result::failed;
    EXPECT_CALL(host, ReadDcsMock(0x04, 3)).WillOnce(testing::Return(std::vector<uint8_t>{}));
    EXPECT_CALL(initialized, callback(InitializationResult::noResponse));
    Create();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_DEATH(panel->Sleep(onDone), "");
    EXPECT_DEATH(panel->Wake(onDone), "");
    EXPECT_DEATH(panel->SetBrightness(1, onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, an_identification_larger_than_the_buffer_asserts)
{
    configuration = dsitest::MakePanel(dsitest::Identification{ 0x04, oversizedIdentification });

    EXPECT_DEATH(Create(), "");
}

TEST_F(MipiDsiPanelCoreTest, sleep_turns_the_display_off_then_enters_sleep_mode)
{
    CreateAndInitialize();
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x28, {});
        EXPECT_CALL(hooks.afterDisplayOff, callback());
        ExpectDcs(host, 0x10, {});
        EXPECT_CALL(done, callback());
    }

    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(200));
}

TEST_F(MipiDsiPanelCoreTest, sleep_completes_after_the_sleep_in_delay)
{
    CreateAndInitialize();
    ExpectDcs(host, 0x28, {});
    EXPECT_CALL(hooks.afterDisplayOff, callback());
    ExpectDcs(host, 0x10, {});

    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(119));
    VerifyAll();

    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, sleep_waits_for_the_after_display_off_hook_before_entering_sleep_mode)
{
    CreateAndInitialize();
    hooks.hold = true;
    ExpectDcs(host, 0x28, {});
    EXPECT_CALL(hooks.afterDisplayOff, callback());

    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(200));
    VerifyAll();

    ExpectDcs(host, 0x10, {});
    hooks.held();
    ForwardTime(std::chrono::milliseconds(119));
    VerifyAll();

    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, wake_exits_sleep_mode_then_turns_the_display_on)
{
    CreateInitializeAndSleep();
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x11, {});
        EXPECT_CALL(hooks.beforeDisplayOn, callback());
        ExpectDcs(host, 0x29, {});
        EXPECT_CALL(done, callback());
    }

    panel->Wake(onDone);
    ForwardTime(std::chrono::milliseconds(200));
}

TEST_F(MipiDsiPanelCoreTest, wake_completes_after_the_sleep_out_delay)
{
    CreateInitializeAndSleep();
    ExpectDcs(host, 0x11, {});

    panel->Wake(onDone);
    ForwardTime(std::chrono::milliseconds(119));
    VerifyAll();

    EXPECT_CALL(hooks.beforeDisplayOn, callback());
    ExpectDcs(host, 0x29, {});
    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, sleep_and_wake_delays_are_configurable)
{
    configuration.timings.displayOffDelay = std::chrono::milliseconds(20);
    configuration.timings.sleepInDelay = std::chrono::milliseconds(30);
    configuration.timings.sleepOutDelay = std::chrono::milliseconds(40);
    configuration.timings.displayOnDelay = std::chrono::milliseconds(10);
    InitializeWith(hal::PixelFormat::rgb565Swapped, InitializationOptions());
    ForwardTime(std::chrono::seconds(1));
    VerifyAll();

    ExpectDcs(host, 0x28, {});
    EXPECT_CALL(hooks.afterDisplayOff, callback());
    ExpectDcs(host, 0x10, {});
    panel->Sleep(onDone);
    ForwardTime(std::chrono::milliseconds(49));
    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
    VerifyAll();

    ExpectDcs(host, 0x11, {});
    EXPECT_CALL(hooks.beforeDisplayOn, callback());
    ExpectDcs(host, 0x29, {});
    panel->Wake(onDone);
    ForwardTime(std::chrono::milliseconds(49));
    EXPECT_CALL(done, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(MipiDsiPanelCoreTest, a_sleep_completion_can_start_wake)
{
    CreateAndInitialize();
    {
        testing::InSequence sequence;
        ExpectDcs(host, 0x28, {});
        EXPECT_CALL(hooks.afterDisplayOff, callback());
        ExpectDcs(host, 0x10, {});
        ExpectDcs(host, 0x11, {});
        EXPECT_CALL(hooks.beforeDisplayOn, callback());
        ExpectDcs(host, 0x29, {});
        EXPECT_CALL(done, callback());
    }

    panel->Sleep([this]()
        {
            panel->Wake(onDone);
        });
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(MipiDsiPanelCoreTest, sleep_while_initializing_asserts)
{
    Create();

    EXPECT_DEATH(panel->Sleep(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, sleep_while_asleep_asserts)
{
    CreateInitializeAndSleep();

    EXPECT_DEATH(panel->Sleep(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, sleep_while_going_to_sleep_asserts)
{
    CreateAndInitialize();
    ExpectDcs(host, 0x28, {});
    panel->Sleep(onDone);

    EXPECT_DEATH(panel->Sleep(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, wake_while_awake_asserts)
{
    CreateAndInitialize();

    EXPECT_DEATH(panel->Wake(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, wake_while_waking_asserts)
{
    CreateInitializeAndSleep();
    ExpectDcs(host, 0x11, {});
    panel->Wake(onDone);

    EXPECT_DEATH(panel->Wake(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, sleep_while_a_brightness_write_is_in_flight_asserts)
{
    CreateAndInitialize();
    ExpectDcs(host, 0x51, { 0x10 });
    panel->SetBrightness(0x10, onDone);

    EXPECT_DEATH(panel->Sleep(onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, brightness_writes_0x51_with_the_given_byte)
{
    CreateAndInitialize();

    for (uint8_t brightness : { 0, 128, 255 })
    {
        ExpectDcs(host, 0x51, { brightness });
        EXPECT_CALL(done, callback());

        panel->SetBrightness(brightness, onDone);
        ExecuteAllActions();
        VerifyAll();
    }
}

TEST_F(MipiDsiPanelCoreTest, brightness_completes_after_the_host_is_done)
{
    CreateAndInitialize();
    host.completeAutomatically = false;
    ExpectDcs(host, 0x51, { 0x40 });

    panel->SetBrightness(0x40, onDone);
    ExecuteAllActions();
    VerifyAll();

    EXPECT_CALL(done, callback());
    host.CompletePending();
}

TEST_F(MipiDsiPanelCoreTest, brightness_while_asleep_asserts)
{
    CreateInitializeAndSleep();

    EXPECT_DEATH(panel->SetBrightness(0x40, onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, brightness_while_initializing_asserts)
{
    Create();

    EXPECT_DEATH(panel->SetBrightness(0x40, onDone), "");
}

TEST_F(MipiDsiPanelCoreTest, brightness_while_another_brightness_write_is_in_flight_asserts)
{
    CreateAndInitialize();
    host.completeAutomatically = false;
    ExpectDcs(host, 0x51, { 0x40 });
    panel->SetBrightness(0x40, onDone);

    EXPECT_DEATH(panel->SetBrightness(0x41, onDone), "");
}
