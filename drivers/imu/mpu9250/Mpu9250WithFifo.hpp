#ifndef DRIVERS_IMU_MPU9250_MPU9250_WITH_FIFO_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_WITH_FIFO_HPP

#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    // The MPU-9250 has no FIFO watermark interrupt; only data-ready and FIFO overflow exist.
    // Batching therefore comes from draining whatever has accumulated since the previous trigger.
    template<class Base, std::size_t MaxFramesPerBatch = 8>
    class Mpu9250WithFifo
        : public Base
    {
    public:
        using Base::Base;

        using Acceleration = typename Base::Acceleration;
        using AngularVelocity = typename Base::AngularVelocity;

        struct FifoConfig
        {
            bool accelerometer = true;
            bool gyroscope = true;
            bool temperature = false;
            bool stopWhenFull = true;
        };

        void EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone);
        void DisableFifo(const infra::Function<void()>& onDone);
        void OnOverflow(const infra::Function<void()>& callback);

    protected:
        void ReadAndDeliverSamples() override;

    private:
        void DrainNextBatch();
        void ConvertAndDeliver(std::size_t frames);
        uint8_t FifoEnableValue() const;
        std::size_t FrameSize() const;

        static constexpr uint8_t userControlFifoEnable = 0x40;
        static constexpr uint8_t userControlFifoReset = 0x04;
        static constexpr uint8_t configurationFifoMode = 0x40;
        static constexpr uint8_t fifoEnableTemperature = 0x80;
        static constexpr uint8_t fifoEnableGyroscope = 0x70;
        static constexpr uint8_t fifoEnableAccelerometer = 0x08;
        static constexpr std::size_t maxFrameSize = 14;

        FifoConfig fifoConfig;
        infra::Function<void()> onOverflow;
        infra::AutoResetFunction<void()> onFifoConfigured;

        std::array<uint8_t, maxFrameSize * MaxFramesPerBatch> fifoBuffer = {};
        std::array<Acceleration, 3 * MaxFramesPerBatch> accelerationBatch = {};
        std::array<AngularVelocity, 3 * MaxFramesPerBatch> angularVelocityBatch = {};
        std::array<uint8_t, 2> fifoCountBuffer = {};

        uint8_t interruptStatus = 0;
        uint16_t remainingBytes = 0;
        bool enabled = false;
    };

    ////    Implementation    ////

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::EnableFifo(const FifoConfig& fifoConfig, const infra::Function<void()>& onDone)
    {
        really_assert(this->sequencer.Finished());

        this->fifoConfig = fifoConfig;
        onFifoConfigured = onDone;

        this->sequencer.Load([this]()
            {
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerFifoEnable, FifoEnableValue(), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->ModifyRegister(Base::registerConfiguration, configurationFifoMode, this->fifoConfig.stopWhenFull ? configurationFifoMode : 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->ModifyRegister(Base::registerUserControl, 0, userControlFifoReset, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->ModifyRegister(Base::registerUserControl, 0, userControlFifoEnable, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Execute([this]()
                    {
                        enabled = true;
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onFifoConfigured();
                            });
                    });
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::DisableFifo(const infra::Function<void()>& onDone)
    {
        really_assert(this->sequencer.Finished());

        onFifoConfigured = onDone;

        this->sequencer.Load([this]()
            {
                this->sequencer.Step([this]()
                    {
                        this->ModifyRegister(Base::registerUserControl, userControlFifoEnable, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerFifoEnable, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Execute([this]()
                    {
                        enabled = false;
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onFifoConfigured();
                            });
                    });
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::OnOverflow(const infra::Function<void()>& callback)
    {
        onOverflow = callback;
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::ReadAndDeliverSamples()
    {
        if (!enabled)
            return Base::ReadAndDeliverSamples();

        this->ReadRegister(Base::registerInterruptStatus, infra::MakeByteRange(interruptStatus), [this]()
            {
                if ((interruptStatus & Base::fifoOverflowInterrupt) != 0)
                    this->ModifyRegister(Base::registerUserControl, 0, userControlFifoReset, [this]()
                        {
                            if (onOverflow)
                                onOverflow();
                        });
                else
                    this->ReadRegister(Base::registerFifoCountHigh, infra::MakeRange(fifoCountBuffer), [this]()
                        {
                            uint16_t count = static_cast<uint16_t>((static_cast<uint16_t>(fifoCountBuffer[0] & 0x1f) << 8) | fifoCountBuffer[1]);
                            remainingBytes = static_cast<uint16_t>(count - count % FrameSize());

                            DrainNextBatch();
                        });
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::DrainNextBatch()
    {
        if (remainingBytes < FrameSize())
            return;

        std::size_t frames = std::min<std::size_t>(remainingBytes / FrameSize(), MaxFramesPerBatch);
        std::size_t bytes = frames * FrameSize();
        remainingBytes = static_cast<uint16_t>(remainingBytes - bytes);

        this->ReadRegister(Base::registerFifoReadWrite, infra::Head(infra::MakeRange(fifoBuffer), bytes), [this, frames]()
            {
                ConvertAndDeliver(frames);
                DrainNextBatch();
            });
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    void Mpu9250WithFifo<Base, MaxFramesPerBatch>::ConvertAndDeliver(std::size_t frames)
    {
        std::size_t accelerationCount = 0;
        std::size_t angularVelocityCount = 0;

        for (std::size_t frame = 0; frame != frames; ++frame)
        {
            const uint8_t* entry = &fifoBuffer[frame * FrameSize()];

            if (fifoConfig.accelerometer)
            {
                for (std::size_t axis = 0; axis != 3; ++axis)
                    accelerationBatch[accelerationCount++] = this->ToAcceleration(Base::RawSample(entry + 2 * axis));

                entry += 6;
            }

            if (fifoConfig.temperature)
                entry += 2;

            if (fifoConfig.gyroscope)
                for (std::size_t axis = 0; axis != 3; ++axis)
                    angularVelocityBatch[angularVelocityCount++] = this->ToAngularVelocity(Base::RawSample(entry + 2 * axis));
        }

        if (accelerationCount != 0)
            this->DeliverAcceleration(infra::Head(infra::MakeRange(accelerationBatch), accelerationCount));

        if (angularVelocityCount != 0)
            this->DeliverAngularVelocity(infra::Head(infra::MakeRange(angularVelocityBatch), angularVelocityCount));
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    uint8_t Mpu9250WithFifo<Base, MaxFramesPerBatch>::FifoEnableValue() const
    {
        uint8_t value = 0;

        if (fifoConfig.temperature)
            value |= fifoEnableTemperature;

        if (fifoConfig.gyroscope)
            value |= fifoEnableGyroscope;

        if (fifoConfig.accelerometer)
            value |= fifoEnableAccelerometer;

        return value;
    }

    template<class Base, std::size_t MaxFramesPerBatch>
    std::size_t Mpu9250WithFifo<Base, MaxFramesPerBatch>::FrameSize() const
    {
        return (fifoConfig.accelerometer ? 6 : 0) + (fifoConfig.temperature ? 2 : 0) + (fifoConfig.gyroscope ? 6 : 0);
    }
}

#endif
