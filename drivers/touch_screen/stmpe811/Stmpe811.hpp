#ifndef DRIVERS_TOUCH_SCREEN_STMPE811_STMPE811_HPP
#define DRIVERS_TOUCH_SCREEN_STMPE811_STMPE811_HPP

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/TouchScreen.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/SharedPtr.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "services/util/RegisterStepRunner.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace drivers
{
    class Stmpe811
        : public hal::TouchScreen
    {
    public:
        enum class InitializationResult : uint8_t
        {
            success,
            deviceNotFound
        };

        struct Config
        {
            infra::Duration pollInterval{ std::chrono::milliseconds(10) };
        };

        static constexpr uint16_t resolution = 4096;

        Stmpe811(services::RegisterBusAccess& bus, hal::GpioPin& interruptPin, const Config& config, const infra::Function<void(InitializationResult)>& onInitialized);
        Stmpe811(const Stmpe811& other) = delete;
        Stmpe811& operator=(const Stmpe811& other) = delete;
        ~Stmpe811();

        hal::TouchScreenSize Size() const override;
        void Start(const infra::Function<void(Event event)>& onTouch) override;
        void Stop(const infra::Function<void()>& onStopped) override;

    private:
        enum class Contact : uint8_t
        {
            none,
            detected,
            reported
        };

        void Identify();
        void ChipIdRead();
        void Configure();
        void ReportInitialized(InitializationResult result);

        void RequestSample();
        void Poll();
        void StatusRead();
        void Released();
        void FetchPoint();
        void PointFetched();
        void FlushFifo();
        void PushFifoFlush();
        void CycleDone();
        void Report(Phase phase, hal::TouchPoint point);
        void Deactivate();
        void ReportStoppedWhenIdle();

    private:
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        services::RegisterStepRunner runner;
        hal::InputPin interruptPin;
        bool interruptConnected;
        Config config;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> stopped;
        infra::TimerSingleShot pollTimer;
        infra::Function<void(Event)> onTouch;

        bool initialized{ false };
        bool started{ false };
        bool sampling{ false };
        bool resample{ false };
        Contact contact{ Contact::none };
        hal::TouchPoint lastPoint{};

        std::array<uint8_t, 2> chipId{};
        uint8_t touchStatus{ 0 };
        uint8_t fifoSize{ 0 };
        std::array<uint8_t, 4> sample{};
    };
}

#endif
