#ifndef DRIVERS_IMU_MPU9250_MPU9250_WITH_SELF_TEST_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_WITH_SELF_TEST_HPP

#include "drivers/imu/mpu9250/Mpu9250Core.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    template<class Base>
    class Mpu9250WithSelfTest
        : public Base
    {
    public:
        using Base::Base;

        struct SelfTestResult
        {
            bool accelerometerX = false;
            bool accelerometerY = false;
            bool accelerometerZ = false;
            bool gyroscopeX = false;
            bool gyroscopeY = false;
            bool gyroscopeZ = false;

            bool Passed() const
            {
                return accelerometerX && accelerometerY && accelerometerZ && gyroscopeX && gyroscopeY && gyroscopeZ;
            }
        };

        static constexpr uint32_t sampleCount = 200;

        void SelfTest(const infra::Function<void(SelfTestResult)>& onDone);

        static uint32_t FactoryTrimResponse(uint8_t trim);
        static bool WithinAcceptanceWindow(int32_t response, uint32_t factoryTrim);

    private:
        void EmitAverageSteps();
        void AccumulateSample();
        void StoreAverage(std::array<int32_t, 6>& destination);
        void EvaluateResult();

        static constexpr uint8_t selfTestEnableAllAxes = 0xe0;
        static constexpr std::size_t configurationSize = 5;

        infra::AutoResetFunction<void(SelfTestResult)> onSelfTestDone;

        std::array<uint8_t, configurationSize> savedConfiguration = {};
        std::array<uint8_t, configurationSize> selfTestConfiguration = { { 0x00, 0x02, 0x00, 0x00, 0x02 } };
        std::array<uint8_t, Base::measurementSize> selfTestBuffer = {};
        std::array<uint8_t, 3> accelerometerTrim = {};
        std::array<uint8_t, 3> gyroscopeTrim = {};
        std::array<int32_t, 6> accumulator = {};
        std::array<int32_t, 6> withoutSelfTest = {};
        std::array<int32_t, 6> withSelfTest = {};
        uint32_t sampleIndex = 0;
        SelfTestResult result;
    };

    ////    Implementation    ////

    template<class Base>
    void Mpu9250WithSelfTest<Base>::SelfTest(const infra::Function<void(SelfTestResult)>& onDone)
    {
        really_assert(this->sequencer.Finished());
        really_assert(!this->Sampling());

        onSelfTestDone = onDone;
        result = SelfTestResult();

        this->sequencer.Load([this]()
            {
                this->sequencer.Step([this]()
                    {
                        this->bus.ReadRegister(Base::registerSampleRateDivider, infra::MakeRange(savedConfiguration), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->bus.WriteRegister(Base::registerSampleRateDivider, infra::MakeRange(selfTestConfiguration), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                EmitAverageSteps();
                this->sequencer.Execute([this]()
                    {
                        StoreAverage(withoutSelfTest);
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerGyroscopeConfig, selfTestEnableAllAxes, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerAccelerometerConfig, selfTestEnableAllAxes, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->delayTimer.Start(std::chrono::milliseconds(20), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                EmitAverageSteps();
                this->sequencer.Execute([this]()
                    {
                        StoreAverage(withSelfTest);
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerGyroscopeConfig, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->WriteRegister(Base::registerAccelerometerConfig, 0, [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->delayTimer.Start(std::chrono::milliseconds(20), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->bus.ReadRegister(Base::registerSelfTestXAccelerometer, infra::MakeRange(accelerometerTrim), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->bus.ReadRegister(Base::registerSelfTestXGyroscope, infra::MakeRange(gyroscopeTrim), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Step([this]()
                    {
                        this->bus.WriteRegister(Base::registerSampleRateDivider, infra::MakeRange(savedConfiguration), [this]()
                            {
                                this->sequencer.Continue();
                            });
                    });
                this->sequencer.Execute([this]()
                    {
                        EvaluateResult();

                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onSelfTestDone(result);
                            });
                    });
            });
    }

    template<class Base>
    void Mpu9250WithSelfTest<Base>::EmitAverageSteps()
    {
        this->sequencer.Execute([this]()
            {
                accumulator.fill(0);
            });
        this->sequencer.ForEach(sampleIndex, 0, sampleCount);
        this->sequencer.Step([this]()
            {
                this->bus.ReadRegister(Base::registerAccelerometerXOutHigh, infra::MakeRange(selfTestBuffer), [this]()
                    {
                        AccumulateSample();
                        this->sequencer.Continue();
                    });
            });
        this->sequencer.EndForEach(sampleIndex);
    }

    template<class Base>
    void Mpu9250WithSelfTest<Base>::AccumulateSample()
    {
        for (std::size_t axis = 0; axis != 3; ++axis)
            accumulator[axis] += Base::RawSample(&selfTestBuffer[2 * axis]);

        for (std::size_t axis = 0; axis != 3; ++axis)
            accumulator[3 + axis] += Base::RawSample(&selfTestBuffer[8 + 2 * axis]);
    }

    template<class Base>
    void Mpu9250WithSelfTest<Base>::StoreAverage(std::array<int32_t, 6>& destination)
    {
        for (std::size_t axis = 0; axis != destination.size(); ++axis)
            destination[axis] = accumulator[axis] / static_cast<int32_t>(sampleCount);
    }

    template<class Base>
    void Mpu9250WithSelfTest<Base>::EvaluateResult()
    {
        std::array<bool*, 6> outcome = { { &result.accelerometerX, &result.accelerometerY, &result.accelerometerZ, &result.gyroscopeX, &result.gyroscopeY, &result.gyroscopeZ } };

        for (std::size_t axis = 0; axis != outcome.size(); ++axis)
        {
            uint8_t trim = axis < 3 ? accelerometerTrim[axis] : gyroscopeTrim[axis - 3];
            *outcome[axis] = WithinAcceptanceWindow(withSelfTest[axis] - withoutSelfTest[axis], FactoryTrimResponse(trim));
        }
    }

    template<class Base>
    uint32_t Mpu9250WithSelfTest<Base>::FactoryTrimResponse(uint8_t trim)
    {
        if (trim == 0)
            return 0;

        uint64_t value = static_cast<uint64_t>(2620) << 16;

        for (uint8_t index = 1; index != trim; ++index)
            value = value * 101 / 100;

        return static_cast<uint32_t>(value >> 16);
    }

    template<class Base>
    bool Mpu9250WithSelfTest<Base>::WithinAcceptanceWindow(int32_t response, uint32_t factoryTrim)
    {
        if (factoryTrim == 0)
            return false;

        // The Y gyroscope response is negative on many parts, so the magnitude is what the datasheet window applies to
        uint32_t magnitude = static_cast<uint32_t>(response < 0 ? -static_cast<int64_t>(response) : response);

        return 2 * magnitude >= factoryTrim && 2 * magnitude <= 3 * factoryTrim;
    }
}

#endif
