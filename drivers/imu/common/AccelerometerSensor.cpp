#include "drivers/imu/common/AccelerometerSensor.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    AccelerometerSensor::AccelerometerAdapter::AccelerometerAdapter(AccelerometerSensor& device)
        : device(device)
    {}

    void AccelerometerSensor::AccelerometerAdapter::Start(const infra::Function<void(Samples)>& onMeasurement)
    {
        really_assert(device.initialized);
        device.onAccelerometerMeasurement = onMeasurement;
        device.UpdateSampling(device.AccelerometerRequested());
    }

    void AccelerometerSensor::AccelerometerAdapter::Stop()
    {
        device.onAccelerometerMeasurement = nullptr;
        device.UpdateSampling(device.AccelerometerRequested());
    }

    AccelerometerSensor::AccelerometerSensor(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin)
        : RegisterSensor(bus, dataReadyPin)
    {}

    AccelerometerSensor::Accelerometer& AccelerometerSensor::AsAccelerometer()
    {
        return accelerometerAdapter;
    }

    void AccelerometerSensor::ClearMeasurementCallbacks()
    {
        onAccelerometerMeasurement = nullptr;
    }

    void AccelerometerSensor::DeliverAcceleration(infra::MemoryRange<const Acceleration> samples)
    {
        if (onAccelerometerMeasurement)
            onAccelerometerMeasurement(samples);
    }

    bool AccelerometerSensor::AccelerometerRequested() const
    {
        return static_cast<bool>(onAccelerometerMeasurement);
    }

    int16_t AccelerometerSensor::RawSample(const uint8_t* data)
    {
        return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8) | data[0]);
    }

    AccelerometerSensor::Acceleration AccelerometerSensor::ToAcceleration(int32_t counts, int64_t microGPerCount)
    {
        static constexpr int64_t gToMmPerSecondSquaredNumerator = 980665;
        static constexpr int64_t microGDenominator = 100'000'000;

        int64_t numerator = static_cast<int64_t>(counts) * microGPerCount * gToMmPerSecondSquaredNumerator;
        int64_t rounding = numerator >= 0 ? microGDenominator / 2 : -(microGDenominator / 2);

        return Acceleration{ static_cast<int32_t>((numerator + rounding) / microGDenominator) };
    }
}
