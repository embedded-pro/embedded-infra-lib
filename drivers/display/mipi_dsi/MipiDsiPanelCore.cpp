#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "drivers/display/mipi_dsi/MipiDcs.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    namespace
    {
        uint8_t ColourMode(hal::PixelFormat format)
        {
            return format == hal::PixelFormat::rgb888 ? dcs::pixelFormat24Bits : dcs::pixelFormat16Bits;
        }
    }

    MipiDsiPanelCore::MipiDsiPanelCore(hal::DsiHost& host, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, infra::MemoryRange<const Command> extraCommands)
        : host(host)
        , panel(panel)
        , extraCommands(extraCommands)
        , colourMode(ColourMode(format))
        , resetPin(reset, true)
        , resetConnected(&reset != &hal::dummyPin)
    {
        really_assert(format != hal::PixelFormat::grey8);
        really_assert(panel.identification.expected.size() <= maxIdentificationSize);
    }

    void MipiDsiPanelCore::Sleep(const infra::Function<void()>& onDone)
    {
        really_assert(phase == Phase::awake);
        BeginHostOperation();

        completion = onDone;
        phase = Phase::goingToSleep;
        RunSequence(infra::MakeRange(sleepSequence));
    }

    void MipiDsiPanelCore::Wake(const infra::Function<void()>& onDone)
    {
        really_assert(phase == Phase::asleep);
        BeginHostOperation();

        completion = onDone;
        phase = Phase::wakingUp;
        RunSequence(infra::MakeRange(wakeSequence));
    }

    void MipiDsiPanelCore::SetBrightness(uint8_t brightness, const infra::Function<void()>& onDone)
    {
        really_assert(phase == Phase::awake);
        BeginHostOperation();

        completion = onDone;
        host.WriteDcs(dcs::writeDisplayBrightness, OwnParameter(brightness), [this]()
            {
                EndHostOperation();
                completion();
            });
    }

    void MipiDsiPanelCore::StartInitialization(const infra::Function<void(InitializationResult)>& onInitialized)
    {
        initialized = onInitialized;
        RunSequence(infra::MakeRange(initializeSequence));
    }

    hal::DsiHost& MipiDsiPanelCore::Host() const
    {
        return host;
    }

    const MipiDsiPanelCore::Panel& MipiDsiPanelCore::Configuration() const
    {
        return panel;
    }

    bool MipiDsiPanelCore::Awake() const
    {
        return phase == Phase::awake;
    }

    void MipiDsiPanelCore::BeginHostOperation()
    {
        really_assert(!hostBusy);
        hostBusy = true;
    }

    void MipiDsiPanelCore::EndHostOperation()
    {
        hostBusy = false;
    }

    void MipiDsiPanelCore::RunSequence(infra::MemoryRange<const Stage> stages)
    {
        sequence = stages;
        stageIndex = 0;
        NextStage();
    }

    void MipiDsiPanelCore::NextStage()
    {
        if (stageIndex == sequence.size())
            SequenceFinished();
        else
            ExecuteStage(sequence[stageIndex++]);
    }

    void MipiDsiPanelCore::ExecuteStage(Stage stage)
    {
        switch (stage)
        {
            case Stage::reset:
                Reset();
                break;
            case Stage::identify:
                Identify();
                break;
            case Stage::beforeSleepOut:
                RunCommands(panel.beforeSleepOut);
                break;
            case Stage::extraCommands:
                RunCommands(extraCommands);
                break;
            case Stage::afterSleepOut:
                RunCommands(panel.afterSleepOut);
                break;
            case Stage::beforeDisplayOn:
                BeforeDisplayOn([this]()
                    {
                        NextStage();
                    });
                break;
            case Stage::afterDisplayOff:
                AfterDisplayOff([this]()
                    {
                        NextStage();
                    });
                break;
            default:
                ExecuteOwnCommandStage(stage);
                break;
        }
    }

    void MipiDsiPanelCore::ExecuteOwnCommandStage(Stage stage)
    {
        const Timings& timings = panel.timings;

        switch (stage)
        {
            case Stage::sleepOut:
                SendOwnCommand(dcs::exitSleepMode, infra::ConstByteRange(), timings.sleepOutDelay);
                break;
            case Stage::pixelFormat:
                SendOwnCommand(dcs::setPixelFormat, OwnParameter(colourMode), infra::Duration::zero());
                break;
            case Stage::addressMode:
                SendOwnCommand(dcs::setAddressMode, OwnParameter(panel.addressMode), infra::Duration::zero());
                break;
            case Stage::displayOn:
                SendOwnCommand(dcs::setDisplayOn, infra::ConstByteRange(), timings.displayOnDelay);
                break;
            case Stage::displayOff:
                SendOwnCommand(dcs::setDisplayOff, infra::ConstByteRange(), timings.displayOffDelay);
                break;
            case Stage::sleepIn:
                SendOwnCommand(dcs::enterSleepMode, infra::ConstByteRange(), timings.sleepInDelay);
                break;
            default:
                break;
        }
    }

    void MipiDsiPanelCore::SequenceFinished()
    {
        EndHostOperation();

        switch (phase)
        {
            case Phase::initializing:
                phase = Phase::awake;
                initialized(InitializationResult::success);
                break;
            case Phase::goingToSleep:
                phase = Phase::asleep;
                completion();
                break;
            case Phase::wakingUp:
                phase = Phase::awake;
                completion();
                break;
            default:
                break;
        }
    }

    void MipiDsiPanelCore::FailInitialization(InitializationResult result)
    {
        EndHostOperation();
        phase = Phase::failed;
        initialized(result);
    }

    void MipiDsiPanelCore::Reset()
    {
        if (resetConnected)
        {
            resetPin.Set(false);
            timer.Start(panel.timings.resetPulse, [this]()
                {
                    ReleaseReset();
                });
        }
        else
            SendOwnCommand(dcs::softReset, infra::ConstByteRange(), panel.timings.resetRecovery);
    }

    void MipiDsiPanelCore::ReleaseReset()
    {
        resetPin.Set(true);
        timer.Start(panel.timings.resetRecovery, [this]()
            {
                NextStage();
            });
    }

    void MipiDsiPanelCore::Identify()
    {
        std::size_t size = panel.identification.expected.size();

        if (size == 0)
            NextStage();
        else
            host.ReadDcs(panel.identification.command, infra::Head(infra::MakeRange(identification), size), [this](hal::DsiHost::Result result)
                {
                    IdentificationRead(result);
                });
    }

    void MipiDsiPanelCore::IdentificationRead(hal::DsiHost::Result result)
    {
        infra::ConstByteRange expected = panel.identification.expected;

        if (result != hal::DsiHost::Result::success)
            FailInitialization(InitializationResult::noResponse);
        else if (!infra::ContentsEqual(infra::Head(infra::MakeRange(identification), expected.size()), expected))
            FailInitialization(InitializationResult::unexpectedId);
        else
            NextStage();
    }

    void MipiDsiPanelCore::RunCommands(infra::MemoryRange<const Command> tableCommands)
    {
        commands = tableCommands;
        commandIndex = 0;
        NextCommand();
    }

    void MipiDsiPanelCore::SendOwnCommand(uint8_t command, infra::ConstByteRange parameters, infra::Duration delayAfter)
    {
        commands = infra::MemoryRange<const Command>();
        commandIndex = 0;
        SendCommand(Packet::dcs, command, parameters, delayAfter);
    }

    void MipiDsiPanelCore::NextCommand()
    {
        if (commandIndex == commands.size())
            NextStage();
        else
        {
            const Command& next = commands[commandIndex++];
            SendCommand(next.packet, next.command, next.parameters, std::chrono::milliseconds(next.delayAfterInMilliseconds));
        }
    }

    void MipiDsiPanelCore::SendCommand(Packet packet, uint8_t command, infra::ConstByteRange parameters, infra::Duration delayAfter)
    {
        really_assert(parameters.size() <= host.MaxParametersSize());
        commandDelay = delayAfter;

        if (packet == Packet::generic)
            host.WriteGeneric(parameters, [this]()
                {
                    CommandWritten();
                });
        else
            host.WriteDcs(command, parameters, [this]()
                {
                    CommandWritten();
                });
    }

    void MipiDsiPanelCore::CommandWritten()
    {
        if (commandDelay == infra::Duration::zero())
            NextCommand();
        else
            timer.Start(commandDelay, [this]()
                {
                    NextCommand();
                });
    }

    infra::ConstByteRange MipiDsiPanelCore::OwnParameter(uint8_t value)
    {
        ownParameter[0] = value;
        return infra::MakeRange(ownParameter);
    }
}
