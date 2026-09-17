#ifndef DRIVERS_IMU_LSM303DLHC_LSM303DLHC_ACCELEROMETER_WITH_FIFO_HPP
#define DRIVERS_IMU_LSM303DLHC_LSM303DLHC_ACCELEROMETER_WITH_FIFO_HPP

#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometer.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    // The buffer holds thirty-two slots of X, Y and Z, drained through the output registers where
    // every six bytes pop one slot. Emptying it requires a pass through bypass mode.
    template<class Base, std::size_t MaxFramesPerBatch = 8>
    class Lsm303dlhcAccelerometerWithFifo
        : public Base
    {
    public:
        using Base::Base;

        using Acceleration = typename Base::Acceleration;

        enum class FifoMode : uint8_t
        {
            bypass = 0,
            fifo = 1,
            stream = 2,
            triggered = 3
        };

        struct FifoConfig
        {
            FifoMode mode = FifoMode::stream;
            uint8_t watermark = 16;
            bool interruptOnWatermark = true;
            bool interruptOnOverrun = true;
        };

        void EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone);
        void DisableFifo(const infra::Function<void()>& onDone);
        void OnOverrun(const infra::Function<void()>& callback);

    protected:
        void ReadAndDeliverSamples() override;

    private:
        void DrainNextBatch();
        void ConvertAndDeliver(std::size_t frames);
        uint8_t FifoControlValue() const;
        uint8_t InterruptMaskValue() const;

        static constexpr uint8_t overrunFlag = 0x40;
        static constexpr uint8_t storedSamplesMask = 0x1f;
        static constexpr uint8_t maximumWatermark = 32;
        static constexpr std::size_t frameSize = 6;

        FifoConfig fifoConfig;
        infra::Function<void()> onOverrun;
        infra::AutoResetFunction<void()> onFifoConfigured;

        std::array<uint8_t, frameSize * MaxFramesPerBatch> fifoBuffer = {};
        std::array<Acceleration, 3 * MaxFramesPerBatch> accelerationBatch = {};

        uint8_t fifoSource = 0;
        uint8_t remainingFrames = 0;
        std::size_t framesInBatch = 0;
        bool enabled = false;
    };

    ////    Implementation    ////

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone)
    {
        really_assert(!this->runner.Busy());
        really_assert(!this->Sampling());
        really_assert(fifoConfig.watermark < maximumWatermark);

        this->fifoConfig = fifoConfig;
        onFifoConfigured = onDone;

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, 0 });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl5, Base::fifoEnable, Base::fifoEnable });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, FifoControlValue() });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, static_cast<uint8_t>(Base::watermarkInterrupt1 | Base::overrunInterrupt1), InterruptMaskValue() });

        this->runner.Start([this]()
            {
                enabled = true;
                onFifoConfigured();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::DisableFifo(const infra::Function<void()>& onDone)
    {
        really_assert(!this->runner.Busy());
        really_assert(!this->Sampling());

        onFifoConfigured = onDone;

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, static_cast<uint8_t>(Base::watermarkInterrupt1 | Base::overrunInterrupt1), 0 });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, 0 });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl5, Base::fifoEnable, 0 });

        this->runner.Start([this]()
            {
                enabled = false;
                onFifoConfigured();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::OnOverrun(const infra::Function<void()>& callback)
    {
        onOverrun = callback;
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::ReadAndDeliverSamples()
    {
        if (!enabled)
            return Base::ReadAndDeliverSamples();

        this->ReadRegister(Base::registerFifoSource, infra::MakeByteRange(fifoSource), [self = this->KeepAlive(*this)]()
            {
                if ((self->fifoSource & overrunFlag) != 0)
                    self->WriteRegister(Base::registerFifoControl, 0, [self]()
                        {
                            self->WriteRegister(Base::registerFifoControl, self->FifoControlValue(), [self]()
                                {
                                    if (self->onOverrun)
                                        self->onOverrun();
                                });
                        });
                else
                {
                    self->remainingFrames = self->fifoSource & storedSamplesMask;
                    self->DrainNextBatch();
                }
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::DrainNextBatch()
    {
        if (remainingFrames == 0)
            return;

        framesInBatch = std::min<std::size_t>(remainingFrames, MaxFramesPerBatch);
        remainingFrames = static_cast<uint8_t>(remainingFrames - framesInBatch);

        this->ReadRegister(Base::registerOutXLow, infra::Head(infra::MakeRange(fifoBuffer), framesInBatch * frameSize), [self = this->KeepAlive(*this)]()
            {
                self->ConvertAndDeliver(self->framesInBatch);
                self->DrainNextBatch();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::ConvertAndDeliver(std::size_t frames)
    {
        std::size_t count = 0;

        for (std::size_t frame = 0; frame != frames; ++frame)
        {
            const uint8_t* entry = &fifoBuffer[frame * frameSize];

            for (std::size_t axis = 0; axis != 3; ++axis)
                accelerationBatch[count++] = this->ToAcceleration(Base::RawSample(entry + 2 * axis));
        }

        this->DeliverAcceleration(infra::Head(infra::MakeRange(accelerationBatch), count));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::FifoControlValue() const
    {
        return static_cast<uint8_t>((static_cast<uint8_t>(fifoConfig.mode) << 6) | (fifoConfig.watermark & storedSamplesMask));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t Lsm303dlhcAccelerometerWithFifo<Base, MaxFramesPerBatch>::InterruptMaskValue() const
    {
        return static_cast<uint8_t>((fifoConfig.interruptOnWatermark ? Base::watermarkInterrupt1 : 0) | (fifoConfig.interruptOnOverrun ? Base::overrunInterrupt1 : 0));
    }
}

#endif
