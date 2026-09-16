#include "hal/interfaces/test_doubles/GyroscopeMock.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    using AngularVelocity = infra::Quantity<infra::MilliDegreePerSecond, int32_t>;
    using Gyroscope = hal::Gyroscope<infra::MilliDegreePerSecond, int32_t>;
    using GyroscopeMock = hal::GyroscopeMock<infra::MilliDegreePerSecond, int32_t>;
    using Samples = Gyroscope::Samples;

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

    class GyroscopeTest
        : public testing::Test
    {
    public:
        void Start()
        {
            EXPECT_CALL(gyroscope, Start(testing::_)).WillOnce([this](const infra::Function<void(Samples)>& callback)
                {
                    onMeasurement = callback;
                });

            gyroscope.Start([this](Samples samples)
                {
                    recorder.Record(samples);
                });
        }

        testing::StrictMock<GyroscopeMock> gyroscope;
        infra::Function<void(Samples)> onMeasurement;
        Recorder recorder;
    };
}

TEST_F(GyroscopeTest, Start_registers_callback)
{
    EXPECT_CALL(gyroscope, Start(testing::_));

    gyroscope.Start([](Samples) {});
}

TEST_F(GyroscopeTest, Stop_halts_measurements)
{
    EXPECT_CALL(gyroscope, Stop());

    gyroscope.Stop();
}

TEST_F(GyroscopeTest, registered_callback_receives_one_sample_per_axis)
{
    Start();

    std::array<AngularVelocity, 3> axes{ AngularVelocity{ 1 }, AngularVelocity{ -2 }, AngularVelocity{ 8750 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(3), recorder.size);
    EXPECT_EQ(1, recorder.values[0]);
    EXPECT_EQ(-2, recorder.values[1]);
    EXPECT_EQ(8750, recorder.values[2]);
}

TEST_F(GyroscopeTest, registered_callback_remains_registered_across_bursts)
{
    Start();

    std::array<AngularVelocity, 3> firstBurst{ AngularVelocity{ 1 }, AngularVelocity{ 2 }, AngularVelocity{ 3 } };
    std::array<AngularVelocity, 3> secondBurst{ AngularVelocity{ 4 }, AngularVelocity{ 5 }, AngularVelocity{ 6 } };
    onMeasurement(infra::MakeConstRange(firstBurst));
    onMeasurement(infra::MakeConstRange(secondBurst));

    EXPECT_EQ(2, recorder.bursts);
    EXPECT_EQ(4, recorder.values[0]);
    EXPECT_EQ(5, recorder.values[1]);
    EXPECT_EQ(6, recorder.values[2]);
}

TEST_F(GyroscopeTest, number_of_axes_is_not_fixed_by_the_interface)
{
    Start();

    std::array<AngularVelocity, 6> axes{ AngularVelocity{ 1 }, AngularVelocity{ 2 }, AngularVelocity{ 3 }, AngularVelocity{ 4 }, AngularVelocity{ 5 }, AngularVelocity{ 6 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(6), recorder.size);
}

TEST_F(GyroscopeTest, measurements_are_dispatched_through_the_interface)
{
    Gyroscope& sensor = gyroscope;

    EXPECT_CALL(gyroscope, Start(testing::_));
    EXPECT_CALL(gyroscope, Stop());

    sensor.Start([](Samples) {});
    sensor.Stop();
}

TEST(GyroscopeInstantiationTest, interface_supports_other_unit_and_storage_combinations)
{
    testing::StrictMock<hal::GyroscopeMock<infra::DegreePerSecond, float>> gyroscope;

    EXPECT_CALL(gyroscope, Stop());

    gyroscope.Stop();
}
