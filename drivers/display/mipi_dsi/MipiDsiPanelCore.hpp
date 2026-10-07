#ifndef DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_PANEL_CORE_HPP
#define DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_PANEL_CORE_HPP

#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/DsiHost.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/MemoryRange.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class MipiDsiPanelCore
    {
    public:
        enum class Packet : uint8_t
        {
            dcs,
            generic
        };

        // A generic command carries its complete payload in parameters and ignores command
        struct Command
        {
            Packet packet;
            uint8_t command;
            infra::ConstByteRange parameters;
            uint16_t delayAfterInMilliseconds;
        };

        struct Identification
        {
            uint8_t command;
            infra::ConstByteRange expected;
        };

        struct Timings
        {
            infra::Duration resetPulse{ std::chrono::milliseconds(10) };
            infra::Duration resetRecovery{ std::chrono::milliseconds(120) };
            infra::Duration sleepOutDelay{ std::chrono::milliseconds(120) };
            infra::Duration sleepInDelay{ std::chrono::milliseconds(120) };
            infra::Duration displayOnDelay{ std::chrono::milliseconds(0) };
            infra::Duration displayOffDelay{ std::chrono::milliseconds(0) };
            infra::Duration tearingEffectTimeout{ std::chrono::milliseconds(100) };
        };

        // size is the logical size after the address mode has been applied; addressMode is the raw parameter of set_address_mode
        struct Panel
        {
            hal::DisplaySize size;
            uint8_t addressMode;
            Identification identification;
            infra::MemoryRange<const Command> beforeSleepOut;
            infra::MemoryRange<const Command> afterSleepOut;
            Timings timings;
        };

        enum class InitializationResult : uint8_t
        {
            success,
            noResponse,
            unexpectedId
        };

        void Sleep(const infra::Function<void()>& onDone);
        void Wake(const infra::Function<void()>& onDone);
        void SetBrightness(uint8_t brightness, const infra::Function<void()>& onDone);

    protected:
        MipiDsiPanelCore(hal::DsiHost& host, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, infra::MemoryRange<const Command> extraCommands);
        MipiDsiPanelCore(const MipiDsiPanelCore& other) = delete;
        MipiDsiPanelCore& operator=(const MipiDsiPanelCore& other) = delete;
        ~MipiDsiPanelCore() = default;

        void StartInitialization(const infra::Function<void(InitializationResult)>& onInitialized);

        virtual void BeforeDisplayOn(const infra::Function<void()>& onDone) = 0;
        virtual void AfterDisplayOff(const infra::Function<void()>& onDone) = 0;

        hal::DsiHost& Host() const;
        const Panel& Configuration() const;
        bool Awake() const;
        void BeginHostOperation();
        void EndHostOperation();

    private:
        enum class Phase : uint8_t
        {
            initializing,
            awake,
            goingToSleep,
            asleep,
            wakingUp,
            failed
        };

        enum class Stage : uint8_t
        {
            reset,
            identify,
            beforeSleepOut,
            sleepOut,
            pixelFormat,
            addressMode,
            extraCommands,
            afterSleepOut,
            beforeDisplayOn,
            displayOn,
            displayOff,
            afterDisplayOff,
            sleepIn
        };

        static constexpr std::size_t maxIdentificationSize = 8;
        static constexpr std::array<Stage, 10> initializeSequence{ { Stage::reset, Stage::identify, Stage::beforeSleepOut, Stage::sleepOut, Stage::pixelFormat, Stage::addressMode, Stage::extraCommands, Stage::afterSleepOut, Stage::beforeDisplayOn, Stage::displayOn } };
        static constexpr std::array<Stage, 3> sleepSequence{ { Stage::displayOff, Stage::afterDisplayOff, Stage::sleepIn } };
        static constexpr std::array<Stage, 3> wakeSequence{ { Stage::sleepOut, Stage::beforeDisplayOn, Stage::displayOn } };

        void RunSequence(infra::MemoryRange<const Stage> stages);
        void NextStage();
        void ExecuteStage(Stage stage);
        void ExecuteOwnCommandStage(Stage stage);
        void SequenceFinished();
        void FailInitialization(InitializationResult result);

        void Reset();
        void ReleaseReset();
        void Identify();
        void IdentificationRead(hal::DsiHost::Result result);

        void RunCommands(infra::MemoryRange<const Command> tableCommands);
        void SendOwnCommand(uint8_t command, infra::ConstByteRange parameters, infra::Duration delayAfter);
        void NextCommand();
        void SendCommand(Packet packet, uint8_t command, infra::ConstByteRange parameters, infra::Duration delayAfter);
        void CommandWritten();
        infra::ConstByteRange OwnParameter(uint8_t value);

    private:
        hal::DsiHost& host;
        const Panel& panel;
        infra::MemoryRange<const Command> extraCommands;
        uint8_t colourMode;
        hal::OutputPin resetPin;
        const bool resetConnected;
        infra::TimerSingleShot timer;
        infra::AutoResetFunction<void(InitializationResult)> initialized;
        infra::AutoResetFunction<void()> completion;
        Phase phase{ Phase::initializing };
        bool hostBusy{ true };

        infra::MemoryRange<const Stage> sequence;
        std::size_t stageIndex{ 0 };
        infra::MemoryRange<const Command> commands;
        std::size_t commandIndex{ 0 };
        infra::Duration commandDelay{ infra::Duration::zero() };
        std::array<uint8_t, 1> ownParameter{};
        std::array<uint8_t, maxIdentificationSize> identification{};
    };
}

#endif
