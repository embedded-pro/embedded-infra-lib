#ifndef DRIVERS_IMU_L3GD20_L3GD20_WITH_SELF_TEST_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_WITH_SELF_TEST_HPP

#include "drivers/imu/l3gd20/L3gd20Core.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <cstdlib>

namespace drivers
{
    // Applies the electrostatic test force and checks that every axis moves by the amount the
    // datasheet promises. The sign of the response differs between the two parts and between the two
    // test polarities, so the magnitude of the difference is what is judged.
    template<class Base>
    class L3gd20WithSelfTest
        : public Base
    {
    public:
        using Base::Base;

        struct SelfTestResult
        {
            bool x = false;
            bool y = false;
            bool z = false;

            // Counts at the 2000 dps scale the procedure forces
            std::array<int32_t, 3> delta = {};

            bool Passed() const
            {
                return x && y && z;
            }
        };

        static constexpr uint32_t sampleCount = 5;

        // 175 to 875 degrees per second at 70 milli-degrees per count
        static constexpr int32_t defaultMinimumDelta = 2500;
        static constexpr int32_t defaultMaximumDelta = 12500;

        void SelfTest(const infra::Function<void(SelfTestResult)>& onDone);
        void SetAcceptanceWindow(int32_t minimum, int32_t maximum);
        void SetSettlingTimes(infra::Duration afterConfiguration, infra::Duration afterPolarityChange);

        static bool WithinAcceptanceWindow(int32_t delta, int32_t minimum, int32_t maximum);

    private:
        void BeginAveraging();
        void ReadNextSample();
        void AccumulateSample();
        void ReportResult();
        int32_t Delta(std::size_t axis) const;

        static constexpr std::size_t axisCount = 3;

        // 200 Hz, all axes on, block data update with the widest scale, every interrupt source off
        std::array<uint8_t, Base::controlRegisterCount> selfTestConfiguration = { { 0x6f, 0x00, 0x00, 0xa0, 0x00 } };
        std::array<uint8_t, Base::controlRegisterCount> savedConfiguration = {};
        std::array<uint8_t, Base::measurementSize> selfTestBuffer = {};
        std::array<int32_t, axisCount> accumulator = {};
        std::array<int32_t, axisCount> withoutSelfTest = {};
        std::array<int32_t, axisCount> withSelfTest = {};

        infra::AutoResetFunction<void(SelfTestResult)> onSelfTestDone;

        uint8_t savedLowOutputDataRate = 0;

        infra::Duration configurationSettlingTime{ std::chrono::milliseconds(800) };
        infra::Duration polaritySettlingTime{ std::chrono::milliseconds(60) };
        int32_t minimumDelta = defaultMinimumDelta;
        int32_t maximumDelta = defaultMaximumDelta;
        uint32_t sampleIndex = 0;
        bool measuringSelfTest = false;
    };

    ////    Implementation    ////

    template<class Base>
    void L3gd20WithSelfTest<Base>::SetAcceptanceWindow(int32_t minimum, int32_t maximum)
    {
        really_assert(minimum >= 0);
        really_assert(minimum <= maximum);

        minimumDelta = minimum;
        maximumDelta = maximum;
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::SetSettlingTimes(infra::Duration afterConfiguration, infra::Duration afterPolarityChange)
    {
        configurationSettlingTime = afterConfiguration;
        polaritySettlingTime = afterPolarityChange;
    }

    template<class Base>
    bool L3gd20WithSelfTest<Base>::WithinAcceptanceWindow(int32_t delta, int32_t minimum, int32_t maximum)
    {
        int32_t magnitude = std::abs(delta);

        return magnitude >= minimum && magnitude <= maximum;
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::SelfTest(const infra::Function<void(SelfTestResult)>& onDone)
    {
        really_assert(this->Initialized());
        really_assert(!this->runner.Busy());
        really_assert(!this->Sampling());

        onSelfTestDone = onDone;
        measuringSelfTest = false;

        auto control4 = selfTestConfiguration[3];
        bool suspendsLowOutputDataRate = this->HasLowOutputDataRateRegister();

        this->runner.Clear();
        this->runner.Push(services::RegisterStepRunner::ReadBurst{ Base::registerControl1, infra::MakeRange(savedConfiguration) });

        if (suspendsLowOutputDataRate)
            this->runner.Push(services::RegisterStepRunner::ReadBurst{ Base::registerLowOutputDataRate, infra::MakeByteRange(savedLowOutputDataRate) });

        this->runner.Push(services::RegisterStepRunner::WriteBurst{ Base::registerControl1, infra::MakeRange(selfTestConfiguration) });

        if (suspendsLowOutputDataRate)
            this->runner.Push(services::RegisterStepRunner::ModifyRegister{ Base::registerLowOutputDataRate, Base::lowOutputDataRateEnable, 0 });
        this->runner.Push(services::RegisterStepRunner::Delay{ configurationSettlingTime });
        this->runner.Push(services::RegisterStepRunner::Await{ [this]()
            {
                BeginAveraging();
            } });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerControl4, static_cast<uint8_t>(control4 | Base::selfTestPositive) });
        this->runner.Push(services::RegisterStepRunner::Delay{ polaritySettlingTime });
        this->runner.Push(services::RegisterStepRunner::Await{ [this]()
            {
                BeginAveraging();
            } });
        this->runner.Push(services::RegisterStepRunner::WriteRegister{ Base::registerControl4, control4 });
        this->runner.Push(services::RegisterStepRunner::Delay{ polaritySettlingTime });
        this->runner.Push(services::RegisterStepRunner::WriteBurst{ Base::registerControl1, infra::MakeRange(savedConfiguration) });

        // A burst over the saved byte, because WriteRegister would capture its value now, while the
        // register has not been read back yet
        if (suspendsLowOutputDataRate)
            this->runner.Push(services::RegisterStepRunner::WriteBurst{ Base::registerLowOutputDataRate, infra::MakeByteRange(savedLowOutputDataRate) });

        this->runner.Start([this]()
            {
                ReportResult();
            });
    }

    template<class Base>
    int32_t L3gd20WithSelfTest<Base>::Delta(std::size_t axis) const
    {
        return withSelfTest[axis] - withoutSelfTest[axis];
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::ReportResult()
    {
        SelfTestResult result;

        for (std::size_t axis = 0; axis != axisCount; ++axis)
            result.delta[axis] = Delta(axis);

        result.x = WithinAcceptanceWindow(result.delta[0], minimumDelta, maximumDelta);
        result.y = WithinAcceptanceWindow(result.delta[1], minimumDelta, maximumDelta);
        result.z = WithinAcceptanceWindow(result.delta[2], minimumDelta, maximumDelta);

        onSelfTestDone(result);
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::BeginAveraging()
    {
        accumulator = {};
        sampleIndex = 0;

        ReadNextSample();
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::ReadNextSample()
    {
        this->ReadRegister(Base::registerOutXLow, infra::MakeRange(selfTestBuffer), [self = this->KeepAlive(*this)]()
            {
                self->AccumulateSample();
            });
    }

    template<class Base>
    void L3gd20WithSelfTest<Base>::AccumulateSample()
    {
        // The first reading after a configuration change is taken while the output is still moving
        if (sampleIndex != 0)
            for (std::size_t axis = 0; axis != axisCount; ++axis)
                accumulator[axis] += Base::RawSample(&selfTestBuffer[2 * axis]);

        ++sampleIndex;

        if (sampleIndex <= sampleCount)
            return ReadNextSample();

        auto& destination = measuringSelfTest ? withSelfTest : withoutSelfTest;

        for (std::size_t axis = 0; axis != axisCount; ++axis)
            destination[axis] = accumulator[axis] / static_cast<int32_t>(sampleCount);

        measuringSelfTest = true;

        this->runner.Continue();
    }
}

#endif
