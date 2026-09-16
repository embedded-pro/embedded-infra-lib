#include "infra/util/Unit.hpp"
#include "gtest/gtest.h"

typedef infra::Quantity<infra::Meter, int> Meter;
typedef infra::Quantity<infra::MilliMeter, int> MilliMeter;

typedef infra::Quantity<infra::Second, int> Second;
typedef infra::Quantity<infra::Minute, int> Minute;
typedef infra::Quantity<infra::Hour, int> Hour;
typedef infra::Quantity<infra::Day, int> Day;
typedef infra::Quantity<infra::MilliSecond, int> MilliSecond;
typedef infra::Quantity<infra::MicroSecond, int> MicroSecond;

typedef infra::Quantity<infra::MeterPerSecond, int> MeterPerSecond;
typedef infra::Quantity<infra::MeterPerSecondSquared, int> MeterPerSecondSquared;
typedef infra::Quantity<infra::MilliMeterPerSecondSquared, int> MilliMeterPerSecondSquared;

typedef infra::Quantity<infra::Degree, int> Degree;
typedef infra::Quantity<infra::DegreePerSecond, int> DegreePerSecond;
typedef infra::Quantity<infra::MilliDegreePerSecond, int> MilliDegreePerSecond;

typedef infra::Quantity<infra::Tesla, int> Tesla;
typedef infra::Quantity<infra::MicroTesla, int> MicroTesla;
typedef infra::Quantity<infra::Gauss, int> Gauss;
typedef infra::Quantity<infra::MilliGauss, int> MilliGauss;

typedef infra::Quantity<infra::Celsius, int> Celsius;
typedef infra::Quantity<infra::MilliCelsius, int> MilliCelsius;

typedef infra::Quantity<infra::MilliVolt, int> MilliVolt;
typedef infra::Quantity<infra::MicroVolt, int> MicroVolt;
typedef infra::Quantity<infra::MilliAmpere, int> MilliAmpere;
typedef infra::Quantity<infra::MicroAmpere, int> MicroAmpere;
typedef infra::Quantity<infra::KiloOhm, int> KiloOhm;

TEST(UnitTest, ADCUseCase)
{
    // ADC range is 0 - 65535, which corresponds to 0 - 1.023V
    MicroVolt adcToMicroVoltFactor(1024 * 1000 / 65536);

    int adcResult = 4096; // 1/16 of the ADC range, so corresponds with 0.064V
    MilliVolt voltageAdc = adcResult * adcToMicroVoltFactor;

    // Assume a resistor of 5K from ADC to ground, and a resistor of 10K from ADC to the measured point.
    // In order to calculate the voltage of the measured point:
    KiloOhm resistorAdcToGround(5);
    KiloOhm resistorAdcToMeasured(10);
    MicroAmpere currentThroughResistorToGround = voltageAdc / resistorAdcToGround;
    MilliVolt voltageAcrossResistorToMeasured = currentThroughResistorToGround * resistorAdcToMeasured;
    MilliVolt voltageOfMeasured = voltageAcrossResistorToMeasured + voltageAdc;

    //    voltageOfMeasured = currentThroughResistorToGround * resistorAdcToMeasured + voltageAdc;
}

TEST(UnitTest, DefaultCreation)
{
    Meter distance;

    EXPECT_EQ(0, distance.Value());
}

TEST(UnitTest, CreationWithValue)
{
    Meter distance(20);

    EXPECT_EQ(20, distance.Value());
}

TEST(UnitTest, CopyConstruction)
{
    Meter distance(20);
    Meter otherDistance(distance);

    EXPECT_EQ(20, otherDistance.Value());
}

TEST(UnitTest, CopyAssignment)
{
    Meter distance(20);
    Meter otherDistance;
    otherDistance = distance;

    EXPECT_EQ(20, otherDistance.Value());
}

TEST(UnitTest, ConversionToSmallerUnit)
{
    Meter distance(20);
    MilliMeter smallerUnit(distance);

    EXPECT_EQ(20000, smallerUnit.Value());

    Day duration(2);
    MicroSecond smallerDuration(duration);

    EXPECT_EQ(1001308160, smallerDuration.Value());
}

TEST(UnitTest, ConversionToLargerUnit)
{
    MilliMeter distance(20999);
    Meter largerDistance(distance);

    EXPECT_EQ(20, largerDistance.Value());

    MilliSecond duration(14400000);
    Hour largerDuration(duration);

    EXPECT_EQ(4, largerDuration.Value());
}

TEST(UnitTest, Addition)
{
    Meter distance1(5);
    Meter distance2(3);

    Meter sum = distance1 + distance2;

    EXPECT_EQ(8, sum.Value());

    sum = distance1;
    sum += distance2;

    EXPECT_EQ(8, sum.Value());
}

TEST(UnitTest, Subtraction)
{
    Meter distance1(5);
    Meter distance2(3);

    Meter difference = distance1 - distance2;

    EXPECT_EQ(2, difference.Value());

    difference = distance1;
    difference -= distance2;

    EXPECT_EQ(2, difference.Value());
}

TEST(UnitTest, Negation)
{
    Meter distance(4);
    Meter back = -distance;

    EXPECT_EQ(-4, back.Value());
}

TEST(UnitTest, MultiplicationByScalar)
{
    Meter distance(4);
    distance *= 2;
    EXPECT_EQ(8, distance.Value());

    Meter distance2 = distance * 3;
    EXPECT_EQ(24, distance2.Value());

    Meter distance3 = 3 * distance;
    EXPECT_EQ(24, distance3.Value());
}

TEST(UnitTest, DivisionByScalar)
{
    Meter distance(24);
    distance /= 2;
    EXPECT_EQ(12, distance.Value());

    Meter distance2 = distance / 3;
    EXPECT_EQ(4, distance2.Value());

    infra::Quantity<infra::Meter::Inverse, int> inverseDistance = 24 / distance;
    EXPECT_EQ(2, inverseDistance.Value());
}

TEST(UnitTest, Multiplication)
{
    Meter distance(4);
    Second time(2);

    infra::Quantity<infra::Meter::Mul<infra::Second>, int> distanceTime = distance * time;
    EXPECT_EQ(8, distanceTime.Value());

    distanceTime = time * distance;
    EXPECT_EQ(8, distanceTime.Value());

    MeterPerSecond speed(5);
    distance = speed * time;
    EXPECT_EQ(10, distance.Value());
    distance = time * speed;
    EXPECT_EQ(10, distance.Value());
}

TEST(UnitTest, Division)
{
    Meter distance(24);
    Second time(6);

    MeterPerSecond speed = distance / time;
    EXPECT_EQ(4, speed.Value());

    distance = Meter(20);
    time = distance / speed;
    EXPECT_EQ(5, time.Value());
}

TEST(UnitTest, AccelerationIsSpeedDividedByTime)
{
    MeterPerSecond speed(20);
    Second time(4);

    MeterPerSecondSquared acceleration = speed / time;
    EXPECT_EQ(5, acceleration.Value());
}

TEST(UnitTest, SpeedIsAccelerationMultipliedByTime)
{
    MeterPerSecondSquared acceleration(5);
    Second time(4);

    MeterPerSecond speed = acceleration * time;
    EXPECT_EQ(20, speed.Value());
}

TEST(UnitTest, AccelerationConversionToSmallerUnit)
{
    MilliMeterPerSecondSquared acceleration(MeterPerSecondSquared(2));
    EXPECT_EQ(2000, acceleration.Value());
}

TEST(UnitTest, AngularVelocityIsAngleDividedByTime)
{
    Degree angle(180);
    Second time(2);

    DegreePerSecond angularVelocity = angle / time;
    EXPECT_EQ(90, angularVelocity.Value());

    angle = angularVelocity * time;
    EXPECT_EQ(180, angle.Value());
}

TEST(UnitTest, AngularVelocityConversionToSmallerUnit)
{
    MilliDegreePerSecond angularVelocity(DegreePerSecond(3));
    EXPECT_EQ(3000, angularVelocity.Value());
}

TEST(UnitTest, ImuUseCase)
{
    // A gyroscope with a sensitivity of 8 mdps per LSB, reporting a raw count of 400
    MilliDegreePerSecond sensitivity(8);
    int rawCount = 400;
    MilliDegreePerSecond angularVelocity = rawCount * sensitivity;
    EXPECT_EQ(3200, angularVelocity.Value());

    // Rotating at that rate for 5 seconds
    Second duration(5);
    Degree rotation = angularVelocity * duration;
    EXPECT_EQ(16, rotation.Value());
}

TEST(UnitTest, MagneticFluxDensityConversionBetweenGaussAndTesla)
{
    MicroTesla fromGauss(Gauss(1));
    EXPECT_EQ(100, fromGauss.Value());

    MicroTesla earthField(MilliGauss(500));
    EXPECT_EQ(50, earthField.Value());

    MicroTesla fromTesla(Tesla(3));
    EXPECT_EQ(3000000, fromTesla.Value());
}

TEST(UnitTest, ConversionOfNegativeValueToLargerUnit)
{
    Meter distance(MilliMeter(-20999));
    EXPECT_EQ(-20, distance.Value());

    MicroTesla field(MilliGauss(-500));
    EXPECT_EQ(-50, field.Value());
}

TEST(UnitTest, ConversionOfNegativeValueToSmallerUnit)
{
    MilliMeter distance(Meter(-20));
    EXPECT_EQ(-20000, distance.Value());
}

TEST(UnitTest, TemperatureConversionBetweenCelsiusAndMilliCelsius)
{
    MilliCelsius fromCelsius(Celsius(21));
    EXPECT_EQ(21000, fromCelsius.Value());

    Celsius fromMilliCelsius(MilliCelsius(36600));
    EXPECT_EQ(36, fromMilliCelsius.Value());

    Celsius negative(MilliCelsius(-4500));
    EXPECT_EQ(-4, negative.Value());
}
