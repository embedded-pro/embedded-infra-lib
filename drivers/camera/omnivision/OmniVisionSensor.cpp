#include "drivers/camera/omnivision/OmniVisionSensor.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    OmniVisionSensor::OmniVisionSensor(services::RegisterBusAccess& bus, hal::GpioPin& reset,
        hal::GpioPin& powerDown, hal::Camera& capture,
        const Descriptor& descriptor, hal::CameraFormat format,
        const Timings& timings,
        const infra::Function<void(InitializationResult)>& onInitialized)
        : bus(bus)
        , resetPin(reset, false)
        , powerDownPin(powerDown, true)
        , capture(capture)
        , descriptor(descriptor)
        , format(format)
        , timings(timings)
        , onInitialized(onInitialized)
        , runner(bus)
    {
        timer.Start(timings.powerUp, [this]()
            {
                AfterPowerUp();
            });
    }

    OmniVisionSensor::~OmniVisionSensor()
    {
        really_assert(!runner.Busy() && !busPending);

        if (captureStarted)
            capture.Stop();
    }

    void OmniVisionSensor::Start(hal::CameraFormat cameraFormat, Mode mode, infra::ByteRange buffer,
        const infra::Function<void(Frame)>& onFrame,
        const infra::Function<void(Error)>& onError)
    {
        really_assert(phase == Phase::ready);
        really_assert(cameraFormat == format);

        captureStarted = true;
        capture.Start(cameraFormat, mode, buffer, onFrame, onError);
    }

    void OmniVisionSensor::Stop()
    {
        capture.Stop();
        captureStarted = false;
    }

    hal::CameraFormat OmniVisionSensor::Format() const
    {
        return format;
    }

    void OmniVisionSensor::AfterPowerUp()
    {
        phase = Phase::resetPulse;
        powerDownPin.Set(false);
        timer.Start(timings.resetPulse, [this]()
            {
                AfterResetPulse();
            });
    }

    void OmniVisionSensor::AfterResetPulse()
    {
        phase = Phase::resetRecovery;
        resetPin.Set(true);
        timer.Start(timings.resetRecovery, [this]()
            {
                AfterResetRecovery();
            });
    }

    void OmniVisionSensor::AfterResetRecovery()
    {
        phase = Phase::softResetting;
        runner.Run(infra::MemoryRange<const RegisterStep>(&descriptor.softReset, &descriptor.softReset + 1),
            [this]()
            {
                AfterSoftReset();
            });
    }

    void OmniVisionSensor::AfterSoftReset()
    {
        phase = Phase::softResetRecovering;
        timer.Start(timings.softResetRecovery, [this]()
            {
                AfterSoftResetRecovery();
            });
    }

    void OmniVisionSensor::AfterSoftResetRecovery()
    {
        RunBeforeIdentification();
    }

    void OmniVisionSensor::RunBeforeIdentification()
    {
        phase = Phase::beforeIdentification;
        runner.Run(descriptor.beforeIdentification, [this]()
            {
                ReadProductId();
            });
    }

    void OmniVisionSensor::ReadProductId()
    {
        phase = Phase::readingProductId;
        busPending = true;
        bus.ReadRegister(descriptor.productIdRegister, infra::MakeByteRange(productIdBuffer[0]),
            [this]()
            {
                busPending = false;
                ProductIdRead();
            });
    }

    void OmniVisionSensor::ProductIdRead()
    {
        ReadVersion();
    }

    void OmniVisionSensor::ReadVersion()
    {
        phase = Phase::readingVersion;
        busPending = true;
        bus.ReadRegister(descriptor.versionRegister, infra::MakeByteRange(versionBuffer[0]),
            [this]()
            {
                busPending = false;
                VersionRead();
            });
    }

    void OmniVisionSensor::VersionRead()
    {
        CheckIdentification();
    }

    void OmniVisionSensor::CheckIdentification()
    {
        bool idMatch = (productIdBuffer[0] == descriptor.productId);
        bool versionMatch = std::any_of(descriptor.versions.begin(), descriptor.versions.end(),
            [this](uint8_t v)
            {
                return v == versionBuffer[0];
            });

        if (!idMatch || !versionMatch)
        {
            ReportInitialized(InitializationResult::unexpectedId);
            return;
        }

        tableIndex = 0;
        RunNextConfigTable();
    }

    void OmniVisionSensor::RunNextConfigTable()
    {
        phase = Phase::runningTables;

        infra::MemoryRange<const RegisterStep> table;
        switch (tableIndex)
        {
            case 0:
                table = descriptor.base;
                break;
            case 1:
                table = descriptor.format;
                break;
            case 2:
                table = descriptor.resolution;
                break;
            case 3:
                table = descriptor.options;
                break;
            case 4:
                table = descriptor.tuning;
                break;
            default:
                ReportInitialized(InitializationResult::success);
                return;
        }

        runner.Run(table, [this]()
            {
                ConfigTableDone();
            });
    }

    void OmniVisionSensor::ConfigTableDone()
    {
        ++tableIndex;
        RunNextConfigTable();
    }

    void OmniVisionSensor::ReportInitialized(InitializationResult result)
    {
        phase = (result == InitializationResult::success) ? Phase::ready : Phase::failed;
        onInitialized(result);
    }
}
