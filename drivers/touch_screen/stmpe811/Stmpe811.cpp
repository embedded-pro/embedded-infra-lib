#include "drivers/touch_screen/stmpe811/Stmpe811.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    namespace
    {
        constexpr uint8_t chipIdRegister = 0x00;
        constexpr uint8_t systemControl1Register = 0x03;
        constexpr uint8_t systemControl2Register = 0x04;
        constexpr uint8_t interruptControlRegister = 0x09;
        constexpr uint8_t interruptEnableRegister = 0x0a;
        constexpr uint8_t interruptStatusRegister = 0x0b;
        constexpr uint8_t gpioAlternateFunctionRegister = 0x17;
        constexpr uint8_t adcControl1Register = 0x20;
        constexpr uint8_t adcControl2Register = 0x21;
        constexpr uint8_t touchControlRegister = 0x40;
        constexpr uint8_t touchConfigurationRegister = 0x41;
        constexpr uint8_t fifoThresholdRegister = 0x4a;
        constexpr uint8_t fifoStatusRegister = 0x4b;
        constexpr uint8_t fifoSizeRegister = 0x4c;
        constexpr uint8_t touchFractionRegister = 0x56;
        constexpr uint8_t touchDriveRegister = 0x58;
        constexpr uint8_t touchDataNonIncrementingRegister = 0xd7;

        constexpr uint16_t expectedChipId = 0x0811;

        constexpr uint8_t softReset = 0x02;
        constexpr uint8_t softResetReleased = 0x00;
        constexpr uint8_t allBlocksOnExceptTemperatureSensor = 0x08;
        constexpr uint8_t touchPinsAsTouchScreen = 0x0f;

        constexpr uint8_t sampleTime80Cycles = 0x40;
        constexpr uint8_t adc12Bit = 0x08;
        constexpr uint8_t adcControl1Value = sampleTime80Cycles | adc12Bit;
        constexpr uint8_t adcClock3250kHz = 0x01;

        constexpr uint8_t averaging4Samples = 0x80;
        constexpr uint8_t touchDetectDelay500Microseconds = 0x18;
        constexpr uint8_t settling500Microseconds = 0x02;
        constexpr uint8_t touchConfigurationValue = averaging4Samples | touchDetectDelay500Microseconds | settling500Microseconds;

        constexpr uint8_t singleSampleThreshold = 0x01;
        constexpr uint8_t fifoReset = 0x01;
        constexpr uint8_t fifoOperating = 0x00;
        constexpr uint8_t fractionOneBit = 0x01;
        constexpr uint8_t drive50Milliamps = 0x01;
        constexpr uint8_t touchScreenEnabledForPosition = 0x01;

        constexpr uint8_t touchDetectedInterrupt = 0x01;
        constexpr uint8_t fifoThresholdInterrupt = 0x02;
        constexpr uint8_t allInterrupts = 0xff;
        constexpr uint8_t globalInterruptEnabled = 0x01;
        constexpr uint8_t touchDetected = 0x80;

        constexpr std::chrono::milliseconds resetDuration{ 10 };
        constexpr std::chrono::milliseconds settleDuration{ 2 };

        struct ConfigurationStep
        {
            uint8_t address;
            uint8_t value;
            bool settleAfter;
        };

        constexpr std::array<ConfigurationStep, 14> configurationSteps{ {
            { systemControl2Register, allBlocksOnExceptTemperatureSensor, false },
            { gpioAlternateFunctionRegister, touchPinsAsTouchScreen, false },
            { adcControl1Register, adcControl1Value, true },
            { adcControl2Register, adcClock3250kHz, false },
            { touchConfigurationRegister, touchConfigurationValue, false },
            { fifoThresholdRegister, singleSampleThreshold, false },
            { fifoStatusRegister, fifoReset, false },
            { fifoStatusRegister, fifoOperating, false },
            { touchFractionRegister, fractionOneBit, false },
            { touchDriveRegister, drive50Milliamps, false },
            { touchControlRegister, touchScreenEnabledForPosition, false },
            { interruptEnableRegister, touchDetectedInterrupt | fifoThresholdInterrupt, false },
            { interruptStatusRegister, allInterrupts, false },
            { interruptControlRegister, globalInterruptEnabled, true },
        } };

        hal::TouchPoint DecodePoint(const std::array<uint8_t, 4>& sample)
        {
            const auto x = static_cast<uint16_t>((sample[0] << 4) | (sample[1] >> 4));
            const auto y = static_cast<uint16_t>(((sample[1] & 0x0f) << 8) | sample[2]);

            return hal::TouchPoint{ x, y };
        }
    }

    Stmpe811::Stmpe811(services::RegisterBusAccess& bus, hal::GpioPin& interruptPin, const Config& config, const infra::Function<void(InitializationResult)>& onInitialized)
        : runner(bus, sharedAccess)
        , interruptPin(interruptPin)
        , interruptConnected(&interruptPin != &hal::dummyPin)
        , config(config)
        , onInitialized(onInitialized)
    {
        really_assert(config.pollInterval > infra::Duration::zero());

        Identify();
    }

    Stmpe811::~Stmpe811()
    {
        really_assert(!runner.Busy());

        Stop();
    }

    hal::TouchScreenSize Stmpe811::Size() const
    {
        return hal::TouchScreenSize{ resolution, resolution };
    }

    void Stmpe811::Start(const infra::Function<void(Event event)>& onTouch)
    {
        really_assert(initialized);
        really_assert(!started);

        started = true;
        this->onTouch = onTouch;

        if (interruptConnected)
            interruptPin.EnableInterrupt([this]()
                {
                    RequestSample();
                },
                hal::InterruptTrigger::fallingEdge, hal::InterruptType::dispatched);

        RequestSample();
    }

    void Stmpe811::Stop()
    {
        started = false;
        resample = false;
        contact = Contact::none;
        onTouch = nullptr;
        pollTimer.Cancel();

        if (interruptConnected)
            interruptPin.DisableInterrupt();
    }

    void Stmpe811::Identify()
    {
        runner.Clear();
        runner.Push(services::RegisterStepRunner::WriteRegister{ systemControl1Register, softReset });
        runner.Push(services::RegisterStepRunner::Delay{ resetDuration });
        runner.Push(services::RegisterStepRunner::WriteRegister{ systemControl1Register, softResetReleased });
        runner.Push(services::RegisterStepRunner::Delay{ settleDuration });
        runner.Push(services::RegisterStepRunner::ReadBurst{ chipIdRegister, infra::MakeByteRange(chipId) });
        runner.Start([this]()
            {
                ChipIdRead();
            });
    }

    void Stmpe811::ChipIdRead()
    {
        if (((chipId[0] << 8) | chipId[1]) == expectedChipId)
            Configure();
        else
            ReportInitialized(InitializationResult::deviceNotFound);
    }

    void Stmpe811::Configure()
    {
        runner.Clear();

        for (const ConfigurationStep& step : configurationSteps)
        {
            runner.Push(services::RegisterStepRunner::WriteRegister{ step.address, step.value });

            if (step.settleAfter)
                runner.Push(services::RegisterStepRunner::Delay{ settleDuration });
        }

        runner.Start([this]()
            {
                ReportInitialized(InitializationResult::success);
            });
    }

    void Stmpe811::ReportInitialized(InitializationResult result)
    {
        initialized = result == InitializationResult::success;
        onInitialized(result);
    }

    void Stmpe811::RequestSample()
    {
        if (!started)
            return;

        if (sampling)
            resample = true;
        else if (!pollTimer.Armed())
            Poll();
    }

    void Stmpe811::Poll()
    {
        sampling = true;

        runner.Clear();

        if (interruptConnected)
            runner.Push(services::RegisterStepRunner::WriteRegister{ interruptStatusRegister, allInterrupts });

        runner.Push(services::RegisterStepRunner::ReadBurst{ touchControlRegister, infra::MakeByteRange(touchStatus) });
        runner.Push(services::RegisterStepRunner::ReadBurst{ fifoSizeRegister, infra::MakeByteRange(fifoSize) });
        runner.Start([this]()
            {
                StatusRead();
            });
    }

    void Stmpe811::StatusRead()
    {
        if (!started)
            return CycleDone();

        if ((touchStatus & touchDetected) == 0)
            return Released();

        if (contact == Contact::none)
            contact = Contact::detected;

        if (fifoSize != 0)
            FetchPoint();
        else
            CycleDone();
    }

    void Stmpe811::Released()
    {
        const bool pressReported = contact == Contact::reported;
        contact = Contact::none;

        if (pressReported)
            Report(Phase::released, lastPoint);

        if (fifoSize != 0)
            FlushFifo();
        else
            CycleDone();
    }

    void Stmpe811::FetchPoint()
    {
        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ touchDataNonIncrementingRegister, infra::MakeByteRange(sample) });
        PushFifoFlush();
        runner.Start([this]()
            {
                PointFetched();
            });
    }

    void Stmpe811::PointFetched()
    {
        if (!started)
            return CycleDone();

        const hal::TouchPoint point = DecodePoint(sample);
        const bool pressed = contact != Contact::reported;

        if (pressed || !(point == lastPoint))
        {
            contact = Contact::reported;
            lastPoint = point;
            Report(pressed ? Phase::pressed : Phase::moved, point);
        }

        CycleDone();
    }

    void Stmpe811::FlushFifo()
    {
        runner.Clear();
        PushFifoFlush();
        runner.Start([this]()
            {
                CycleDone();
            });
    }

    void Stmpe811::PushFifoFlush()
    {
        runner.Push(services::RegisterStepRunner::WriteRegister{ fifoStatusRegister, fifoReset });
        runner.Push(services::RegisterStepRunner::WriteRegister{ fifoStatusRegister, fifoOperating });
    }

    void Stmpe811::CycleDone()
    {
        sampling = false;

        if (!started)
            return;

        if (resample)
        {
            resample = false;
            Poll();
        }
        else if (contact != Contact::none || !interruptConnected)
            pollTimer.Start(config.pollInterval, [this]()
                {
                    Poll();
                });
    }

    void Stmpe811::Report(Phase phase, hal::TouchPoint point)
    {
        auto callback = onTouch;
        callback(Event{ phase, point });
    }
}
