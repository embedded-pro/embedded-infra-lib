#include "hal/interfaces/test_doubles/AccelerometerMock.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    using Acceleration = infra::Quantity<infra::MilliMeterPerSecondSquared, int32_t>;
    using Accelerometer = hal::Accelerometer<infra::MilliMeterPerSecondSquared, int32_t>;
    using AccelerometerMock = hal::AccelerometerMock<infra::MilliMeterPerSecondSquared, int32_t>;
    using Samples = Accelerometer::Samples;

    struct Recorder
    {
        void Record(Samples samples)
        {
            ++bursts;
            size = samples.size();

            for (std::size_t index = 0; index != samples.size() && index != values.size(); ++index)
                values[index] = samples[index].Value();
        }

        std::array<int32_t, 6> values{};
        std::size_t size{};
        int bursts{};
    };

    class AccelerometerTest
        : public testing::Test
    {
    public:
        void Start()
        {
            EXPECT_CALL(accelerometer, Start(testing::_)).WillOnce([this](const infra::Function<void(Samples)>& callback)
                {
                    onMeasurement = callback;
                });

            accelerometer.Start([this](Samples samples)
                {
                    recorder.Record(samples);
                });
        }

        testing::StrictMock<AccelerometerMock> accelerometer;
        infra::Function<void(Samples)> onMeasurement;
        Recorder recorder;
    };
}

TEST_F(AccelerometerTest, Start_registers_callback)
{
    EXPECT_CALL(accelerometer, Start(testing::_));

    accelerometer.Start([](Samples) {});
}

TEST_F(AccelerometerTest, Stop_halts_measurements)
{
    EXPECT_CALL(accelerometer, Stop());

    accelerometer.Stop();
}

TEST_F(AccelerometerTest, registered_callback_receives_one_sample_per_axis)
{
    Start();

    std::array<Acceleration, 3> axes{ Acceleration{ 1 }, Acceleration{ -2 }, Acceleration{ 9807 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(3), recorder.size);
    EXPECT_EQ(1, recorder.values[0]);
    EXPECT_EQ(-2, recorder.values[1]);
    EXPECT_EQ(9807, recorder.values[2]);
}

TEST_F(AccelerometerTest, registered_callback_remains_registered_across_bursts)
{
    Start();

    std::array<Acceleration, 3> firstBurst{ Acceleration{ 1 }, Acceleration{ 2 }, Acceleration{ 3 } };
    std::array<Acceleration, 3> secondBurst{ Acceleration{ 4 }, Acceleration{ 5 }, Acceleration{ 6 } };
    onMeasurement(infra::MakeConstRange(firstBurst));
    onMeasurement(infra::MakeConstRange(secondBurst));

    EXPECT_EQ(2, recorder.bursts);
    EXPECT_EQ(4, recorder.values[0]);
    EXPECT_EQ(5, recorder.values[1]);
    EXPECT_EQ(6, recorder.values[2]);
}

TEST_F(AccelerometerTest, number_of_axes_is_not_fixed_by_the_interface)
{
    Start();

    std::array<Acceleration, 6> axes{ Acceleration{ 1 }, Acceleration{ 2 }, Acceleration{ 3 }, Acceleration{ 4 }, Acceleration{ 5 }, Acceleration{ 6 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(6), recorder.size);
}

TEST_F(AccelerometerTest, measurements_are_dispatched_through_the_interface)
{
    Accelerometer& sensor = accelerometer;

    EXPECT_CALL(accelerometer, Start(testing::_));
    EXPECT_CALL(accelerometer, Stop());

    sensor.Start([](Samples) {});
    sensor.Stop();
}

TEST(AccelerometerInstantiationTest, interface_supports_other_unit_and_storage_combinations)
{
    testing::StrictMock<hal::AccelerometerMock<infra::MeterPerSecondSquared, float>> accelerometer;

    EXPECT_CALL(accelerometer, Stop());

    accelerometer.Stop();
}
