#ifndef DRIVERS_TOUCH_SCREEN_FT6X06_FT6X06_HPP
#define DRIVERS_TOUCH_SCREEN_FT6X06_FT6X06_HPP

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
    class Ft6x06
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
            infra::Duration pollInterval{ std::chrono::milliseconds(20) };
            infra::Duration identificationRetryInterval{ std::chrono::milliseconds(100) };
            uint8_t identificationAttempts{ 30 };
            hal::TouchScreenSize size{ 480, 800 };
        };

        Ft6x06(services::RegisterBusAccess& bus, const Config& config, const infra::Function<void(InitializationResult)>& onInitialized);
        Ft6x06(const Ft6x06& other) = delete;
        Ft6x06& operator=(const Ft6x06& other) = delete;
        ~Ft6x06();

        hal::TouchScreenSize Size() const override;
        void Start(const infra::Function<void(Event event)>& onTouch) override;
        void Stop(const infra::Function<void()>& onStopped) override;

        uint8_t VendorId() const;
        uint8_t ChipId() const;

    private:
        void Identify();
        void IdentificationRead();
        void ReportInitialized(InitializationResult result);

        void Poll();
        void SampleRead();
        void Report(Phase phase, hal::TouchPoint point);
        hal::TouchPoint DecodePoint() const;
        void ReportStoppedWhenIdle();

    private:
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        services::RegisterStepRunner runner;
        Config config;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> stopped;
        infra::TimerSingleShot retryTimer;
        infra::TimerSingleShot pollTimer;
        infra::Function<void(Event)> onTouch;

        uint8_t attemptsLeft;
        bool initializing{ true };
        bool initialized{ false };
        bool started{ false };
        bool sampling{ false };
        bool touching{ false };
        hal::TouchPoint lastPoint{};

        uint8_t vendorId{ 0 };
        uint8_t chipId{ 0 };
        std::array<uint8_t, 5> sample{};
    };
}

#endif
