#include "hal/interfaces/test_doubles/MagnetometerMock.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    using MagneticFluxDensity = infra::Quantity<infra::MilliGauss, int32_t>;
    using Magnetometer = hal::Magnetometer<infra::MilliGauss, int32_t>;
    using MagnetometerMock = hal::MagnetometerMock<infra::MilliGauss, int32_t>;
    using Samples = Magnetometer::Samples;

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

    class MagnetometerTest
        : public testing::Test
    {
    public:
        void Start()
        {
            EXPECT_CALL(magnetometer, Start(testing::_)).WillOnce([this](const infra::Function<void(Samples)>& callback)
                {
                    onMeasurement = callback;
                });

            magnetometer.Start([this](Samples samples)
                {
                    recorder.Record(samples);
                });
        }

        testing::StrictMock<MagnetometerMock> magnetometer;
        infra::Function<void(Samples)> onMeasurement;
        Recorder recorder;
    };
}

TEST_F(MagnetometerTest, Start_registers_callback)
{
    EXPECT_CALL(magnetometer, Start(testing::_));

    magnetometer.Start([](Samples) {});
}

TEST_F(MagnetometerTest, Stop_halts_measurements)
{
    EXPECT_CALL(magnetometer, Stop());

    magnetometer.Stop();
}

TEST_F(MagnetometerTest, registered_callback_receives_one_sample_per_axis)
{
    Start();

    std::array<MagneticFluxDensity, 3> axes{ MagneticFluxDensity{ 1 }, MagneticFluxDensity{ -2 }, MagneticFluxDensity{ 480 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(3), recorder.size);
    EXPECT_EQ(1, recorder.values[0]);
    EXPECT_EQ(-2, recorder.values[1]);
    EXPECT_EQ(480, recorder.values[2]);
}

TEST_F(MagnetometerTest, registered_callback_remains_registered_across_bursts)
{
    Start();

    std::array<MagneticFluxDensity, 3> firstBurst{ MagneticFluxDensity{ 1 }, MagneticFluxDensity{ 2 }, MagneticFluxDensity{ 3 } };
    std::array<MagneticFluxDensity, 3> secondBurst{ MagneticFluxDensity{ 4 }, MagneticFluxDensity{ 5 }, MagneticFluxDensity{ 6 } };
    onMeasurement(infra::MakeConstRange(firstBurst));
    onMeasurement(infra::MakeConstRange(secondBurst));

    EXPECT_EQ(2, recorder.bursts);
    EXPECT_EQ(4, recorder.values[0]);
    EXPECT_EQ(5, recorder.values[1]);
    EXPECT_EQ(6, recorder.values[2]);
}

TEST_F(MagnetometerTest, number_of_axes_is_not_fixed_by_the_interface)
{
    Start();

    std::array<MagneticFluxDensity, 6> axes{ MagneticFluxDensity{ 1 }, MagneticFluxDensity{ 2 }, MagneticFluxDensity{ 3 }, MagneticFluxDensity{ 4 }, MagneticFluxDensity{ 5 }, MagneticFluxDensity{ 6 } };
    onMeasurement(infra::MakeConstRange(axes));

    EXPECT_EQ(std::size_t(6), recorder.size);
}

TEST_F(MagnetometerTest, measurements_are_dispatched_through_the_interface)
{
    Magnetometer& sensor = magnetometer;

    EXPECT_CALL(magnetometer, Start(testing::_));
    EXPECT_CALL(magnetometer, Stop());

    sensor.Start([](Samples) {});
    sensor.Stop();
}

TEST(MagnetometerInstantiationTest, interface_supports_other_unit_and_storage_combinations)
{
    testing::StrictMock<hal::MagnetometerMock<infra::MicroTesla, float>> magnetometer;

    EXPECT_CALL(magnetometer, Stop());

    magnetometer.Stop();
}
