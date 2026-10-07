#ifndef DRIVERS_CAMERA_OMNIVISION_OMNI_VISION_SENSOR_HPP
#define DRIVERS_CAMERA_OMNIVISION_OMNI_VISION_SENSOR_HPP

#include "drivers/camera/omnivision/RegisterTable.hpp"
#include "drivers/camera/omnivision/RegisterTableRunner.hpp"
#include "hal/interfaces/Camera.hpp"
#include "hal/interfaces/CameraFormat.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace drivers
{
    class OmniVisionSensor
        : public hal::Camera
    {
    public:
        enum class InitializationResult : uint8_t
        {
            success,
            unexpectedId
        };

        struct Timings
        {
            infra::Duration powerUp{ std::chrono::milliseconds(5) };
            infra::Duration resetPulse{ std::chrono::milliseconds(2) };
            infra::Duration resetRecovery{ std::chrono::milliseconds(5) };
            infra::Duration softResetRecovery{ std::chrono::milliseconds(10) };
        };

        struct Descriptor
        {
            uint8_t productIdRegister;
            uint8_t productId;
            uint8_t versionRegister;
            infra::MemoryRange<const uint8_t> versions;
            RegisterStep softReset;
            infra::MemoryRange<const RegisterStep> beforeIdentification;
            infra::MemoryRange<const RegisterStep> base;
            infra::MemoryRange<const RegisterStep> format;
            infra::MemoryRange<const RegisterStep> resolution;
            infra::MemoryRange<const RegisterStep> options;
            infra::MemoryRange<const RegisterStep> tuning;
        };

        void Start(hal::CameraFormat format, Mode mode, infra::ByteRange buffer,
            const infra::Function<void(Frame)>& onFrame,
            const infra::Function<void(Error)>& onError) override;
        void Stop() override;

        hal::CameraFormat Format() const;

    protected:
        OmniVisionSensor(services::RegisterBusAccess& bus, hal::GpioPin& reset,
            hal::GpioPin& powerDown, hal::Camera& capture,
            const Descriptor& descriptor, hal::CameraFormat format,
            const Timings& timings,
            const infra::Function<void(InitializationResult)>& onInitialized);
        OmniVisionSensor(const OmniVisionSensor&) = delete;
        OmniVisionSensor& operator=(const OmniVisionSensor&) = delete;
        ~OmniVisionSensor();

    private:
        enum class Phase : uint8_t
        {
            poweringUp,
            resetPulse,
            resetRecovery,
            softResetting,
            softResetRecovering,
            beforeIdentification,
            readingProductId,
            readingVersion,
            runningTables,
            ready,
            failed
        };

        void AfterPowerUp();
        void AfterResetPulse();
        void AfterResetRecovery();
        void AfterSoftReset();
        void AfterSoftResetRecovery();
        void RunBeforeIdentification();
        void ReadProductId();
        void ProductIdRead();
        void ReadVersion();
        void VersionRead();
        void CheckIdentification();
        void RunNextConfigTable();
        void ConfigTableDone();
        void ReportInitialized(InitializationResult result);

        services::RegisterBusAccess& bus;
        hal::OutputPin resetPin;
        hal::OutputPin powerDownPin;
        hal::Camera& capture;
        Descriptor descriptor;
        hal::CameraFormat format;
        Timings timings;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::TimerSingleShot timer;
        RegisterTableRunner runner;
        Phase phase{ Phase::poweringUp };
        uint8_t tableIndex{ 0 };
        std::array<uint8_t, 1> productIdBuffer{};
        std::array<uint8_t, 1> versionBuffer{};
        bool captureStarted{ false };
        bool busPending{ false };
    };
}

#endif
