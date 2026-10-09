#include "drivers/touch_screen/ft6x06/Ft6x06.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <utility>

namespace drivers
{
    namespace
    {
        constexpr uint8_t touchStatusRegister = 0x02;
        constexpr uint8_t chipIdRegister = 0xa3;
        constexpr uint8_t vendorIdRegister = 0xa8;

        constexpr uint8_t touchCountMask = 0x0f;
        constexpr uint8_t coordinateHighMask = 0x0f;
        constexpr uint8_t maxTouches = 2;

        constexpr uint8_t vendorIdOfAnEmptyBus = 0x00;
        constexpr uint8_t vendorIdOfAFloatingBus = 0xff;

        constexpr std::size_t touchCountIndex = 0;
        constexpr std::size_t xHighIndex = 1;
        constexpr std::size_t xLowIndex = 2;
        constexpr std::size_t yHighIndex = 3;
        constexpr std::size_t yLowIndex = 4;

        uint16_t Coordinate(uint8_t high, uint8_t low, uint16_t size)
        {
            const auto coordinate = static_cast<uint16_t>(((high & coordinateHighMask) << 8) | low);

            return std::min<uint16_t>(coordinate, size - 1);
        }
    }

    Ft6x06::Ft6x06(services::RegisterBusAccess& bus, const Config& config, const infra::Function<void(InitializationResult)>& onInitialized)
        : runner(bus, sharedAccess)
        , config(config)
        , onInitialized(onInitialized)
        , attemptsLeft(config.identificationAttempts)
    {
        really_assert(config.pollInterval > infra::Duration::zero());
        really_assert(config.identificationRetryInterval > infra::Duration::zero());
        really_assert(config.identificationAttempts != 0);
        really_assert(config.size.width != 0 && config.size.height != 0);

        Identify();
    }

    Ft6x06::~Ft6x06()
    {
        really_assert(!stopped);
        really_assert(!runner.Busy());
    }

    hal::TouchScreenSize Ft6x06::Size() const
    {
        if (config.orientation.swapAxes)
            return hal::TouchScreenSize{ config.size.height, config.size.width };

        return config.size;
    }

    void Ft6x06::Start(const infra::Function<void(Event event)>& onTouch)
    {
        really_assert(initialized);
        really_assert(!started);
        really_assert(!stopped);

        started = true;
        touching = false;
        this->onTouch = onTouch;

        Poll();
    }

    void Ft6x06::Stop(const infra::Function<void()>& onStopped)
    {
        really_assert(onStopped != nullptr);
        really_assert(!stopped);

        stopped = onStopped;
        started = false;
        touching = false;
        onTouch = nullptr;
        pollTimer.Cancel();
        ReportStoppedWhenIdle();
    }

    uint8_t Ft6x06::VendorId() const
    {
        return vendorId;
    }

    uint8_t Ft6x06::ChipId() const
    {
        return chipId;
    }

    void Ft6x06::Identify()
    {
        vendorId = 0;
        chipId = 0;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ vendorIdRegister, infra::MakeByteRange(vendorId) });
        runner.Push(services::RegisterStepRunner::ReadBurst{ chipIdRegister, infra::MakeByteRange(chipId) });
        runner.Start([this]()
            {
                IdentificationRead();
            });
    }

    void Ft6x06::IdentificationRead()
    {
        if (vendorId != vendorIdOfAnEmptyBus && vendorId != vendorIdOfAFloatingBus)
            ReportInitialized(InitializationResult::success);
        else if (--attemptsLeft == 0)
            ReportInitialized(InitializationResult::deviceNotFound);
        else
            retryTimer.Start(config.identificationRetryInterval, [this]()
                {
                    Identify();
                });
    }

    void Ft6x06::ReportInitialized(InitializationResult result)
    {
        initializing = false;
        initialized = result == InitializationResult::success;
        ReportStoppedWhenIdle();
        onInitialized(result);
    }

    void Ft6x06::Poll()
    {
        sampling = true;

        runner.Clear();
        runner.Push(services::RegisterStepRunner::ReadBurst{ touchStatusRegister, infra::MakeByteRange(sample) });
        runner.Start([this]()
            {
                SampleRead();
            });
    }

    void Ft6x06::SampleRead()
    {
        sampling = false;

        if (!started)
            return ReportStoppedWhenIdle();

        const uint8_t touches = sample[touchCountIndex] & touchCountMask;

        if (touches != 0 && touches <= maxTouches)
        {
            const hal::TouchPoint point = DecodePoint();
            const bool pressed = !touching;

            touching = true;

            if (pressed || !(point == lastPoint))
            {
                lastPoint = point;
                Report(pressed ? Phase::pressed : Phase::moved, point);
            }
        }
        else if (touches == 0 && touching)
        {
            touching = false;
            Report(Phase::released, lastPoint);
        }

        if (started)
            pollTimer.Start(config.pollInterval, [this]()
                {
                    Poll();
                });
    }

    hal::TouchPoint Ft6x06::DecodePoint() const
    {
        uint16_t x = Coordinate(sample[xHighIndex], sample[xLowIndex], config.size.width);
        uint16_t y = Coordinate(sample[yHighIndex], sample[yLowIndex], config.size.height);

        if (config.orientation.swapAxes)
            std::swap(x, y);

        const hal::TouchScreenSize size = Size();

        if (config.orientation.mirrorX)
            x = static_cast<uint16_t>(size.width - 1 - x);

        if (config.orientation.mirrorY)
            y = static_cast<uint16_t>(size.height - 1 - y);

        return hal::TouchPoint{ x, y };
    }

    void Ft6x06::Report(Phase phase, hal::TouchPoint point)
    {
        auto callback = onTouch;
        callback(Event{ phase, point });
    }

    void Ft6x06::ReportStoppedWhenIdle()
    {
        if (stopped && !initializing && !sampling && !runner.Busy())
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    stopped();
                });
    }
}
