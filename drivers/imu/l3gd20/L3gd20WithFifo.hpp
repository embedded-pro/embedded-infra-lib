#ifndef DRIVERS_IMU_L3GD20_L3GD20_WITH_FIFO_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_WITH_FIFO_HPP

#include "drivers/imu/l3gd20/L3gd20Core.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    // The buffer holds thirty-two slots of X, Y and Z, drained through the output registers where
    // every six bytes pop one slot. Emptying it requires a pass through bypass mode.
    template<class Base, std::size_t MaxFramesPerBatch = 8>
    class L3gd20WithFifo
        : public Base
    {
    public:
        using Base::Base;

        using AngularVelocity = typename Base::AngularVelocity;

        enum class FifoMode : uint8_t
        {
            bypass = 0,
            fifo = 1,
            stream = 2,
            streamToFifo = 3,
            bypassToStream = 4,

            // L3GD20H only
            dynamicStream = 6,
            bypassToFifo = 7
        };

        struct FifoConfig
        {
            FifoMode mode = FifoMode::stream;
            uint8_t watermark = 16;
            bool interruptOnWatermark = true;
            bool interruptOnOverrun = true;
            bool interruptOnEmpty = false;

            // L3GD20H only
            bool stopOnWatermark = false;
        };

        void EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone);
        void DisableFifo(const infra::Function<void()>& onDone);
        void OnOverrun(const infra::Function<void()>& callback);

    protected:
        void ReadAndDeliverSamples() override;
        void ClearCallbacks() override;

    private:
        void DrainNextBatch();
        void ConvertAndDeliver(std::size_t frames);
        void RestoreAfterOverrun();
        uint8_t FifoControlValue() const;
        uint8_t InterruptSourceMask() const;
        uint8_t InterruptSourceValue() const;
        uint8_t Control5Mask() const;
        uint8_t Control5Value() const;

        static constexpr uint8_t maximumWatermark = 32;
        static constexpr std::size_t frameSize = 6;

        FifoConfig fifoConfig;
        infra::Function<void()> onOverrun;
        infra::AutoResetFunction<void()> onFifoConfigured;

        std::array<uint8_t, frameSize * MaxFramesPerBatch> fifoBuffer = {};
        std::array<AngularVelocity, 3 * MaxFramesPerBatch> angularVelocityBatch = {};

        uint8_t fifoSource = 0;
        uint8_t remainingFrames = 0;
        std::size_t framesInBatch = 0;
        bool enabled = false;
    };

    ////    Implementation    ////

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone)
    {
        really_assert(!this->runner.Busy());
        really_assert(!this->Sampling());
        really_assert(fifoConfig.watermark < maximumWatermark);
        really_assert(this->HasLowOutputDataRateRegister() || (fifoConfig.mode != FifoMode::dynamicStream && fifoConfig.mode != FifoMode::bypassToFifo));

        this->fifoConfig = fifoConfig;
        onFifoConfigured = onDone;

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, 0 });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl5, Control5Mask(), Control5Value() });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, FifoControlValue() });
        // Data ready shares this register with the buffer sources, so only the buffer bits move
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, InterruptSourceMask(), InterruptSourceValue() });

        this->runner.Start([this]()
            {
                enabled = true;
                onFifoConfigured();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::DisableFifo(const infra::Function<void()>& onDone)
    {
        really_assert(!this->runner.Busy());
        really_assert(!this->Sampling());

        onFifoConfigured = onDone;

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl3, InterruptSourceMask(), 0 });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerFifoControl, 0 });
        this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerControl5, Control5Mask(), 0 });

        this->runner.Start([this]()
            {
                enabled = false;
                onFifoConfigured();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::OnOverrun(const infra::Function<void()>& callback)
    {
        onOverrun = callback;
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::ClearCallbacks()
    {
        onOverrun = nullptr;

        Base::ClearCallbacks();
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::ReadAndDeliverSamples()
    {
        if (!enabled)
            return Base::ReadAndDeliverSamples();

        this->ReadRegister(Base::registerFifoSource, infra::MakeByteRange(fifoSource), [self = this->KeepAlive(*this)]()
            {
                if ((self->fifoSource & Base::fifoOverrun) != 0)
                    return self->RestoreAfterOverrun();

                if ((self->fifoSource & Base::fifoEmpty) != 0)
                    return;

                self->remainingFrames = self->fifoSource & Base::fifoStoredSamplesMask;
                self->DrainNextBatch();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::RestoreAfterOverrun()
    {
        this->WriteRegister(Base::registerFifoControl, 0, [self = this->KeepAlive(*this)]()
            {
                self->WriteRegister(Base::registerFifoControl, self->FifoControlValue(), [self]()
                    {
                        if (self->onOverrun)
                            self->onOverrun();
                    });
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::DrainNextBatch()
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
    void L3gd20WithFifo<Base, MaxFramesPerBatch>::ConvertAndDeliver(std::size_t frames)
    {
        std::size_t count = 0;

        for (std::size_t frame = 0; frame != frames; ++frame)
        {
            const uint8_t* entry = &fifoBuffer[frame * frameSize];

            for (std::size_t axis = 0; axis != 3; ++axis)
                angularVelocityBatch[count++] = this->ToAngularVelocity(Base::RawSample(entry + 2 * axis));
        }

        this->DeliverAngularVelocity(infra::Head(infra::MakeRange(angularVelocityBatch), count));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t L3gd20WithFifo<Base, MaxFramesPerBatch>::FifoControlValue() const
    {
        return static_cast<uint8_t>((static_cast<uint8_t>(fifoConfig.mode) << 5) | (fifoConfig.watermark & Base::fifoStoredSamplesMask));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t L3gd20WithFifo<Base, MaxFramesPerBatch>::InterruptSourceMask() const
    {
        return static_cast<uint8_t>(Base::watermarkInterrupt2 | Base::overrunInterrupt2 | Base::emptyInterrupt2);
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t L3gd20WithFifo<Base, MaxFramesPerBatch>::InterruptSourceValue() const
    {
        return static_cast<uint8_t>((fifoConfig.interruptOnWatermark ? Base::watermarkInterrupt2 : 0) | (fifoConfig.interruptOnOverrun ? Base::overrunInterrupt2 : 0) | (fifoConfig.interruptOnEmpty ? Base::emptyInterrupt2 : 0));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t L3gd20WithFifo<Base, MaxFramesPerBatch>::Control5Mask() const
    {
        return this->HasLowOutputDataRateRegister() ? static_cast<uint8_t>(Base::fifoEnable | Base::stopOnWatermark) : Base::fifoEnable;
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t L3gd20WithFifo<Base, MaxFramesPerBatch>::Control5Value() const
    {
        return static_cast<uint8_t>(Base::fifoEnable | (this->HasLowOutputDataRateRegister() && fifoConfig.stopOnWatermark ? Base::stopOnWatermark : 0));
    }
}

#endif
