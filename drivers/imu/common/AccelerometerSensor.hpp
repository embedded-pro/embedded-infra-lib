#ifndef DRIVERS_IMU_COMMON_ACCELEROMETER_SENSOR_HPP
#define DRIVERS_IMU_COMMON_ACCELEROMETER_SENSOR_HPP

#include "drivers/imu/common/RegisterSensor.hpp"
#include "hal/interfaces/Accelerometer.hpp"
#include "infra/util/Unit.hpp"
#include <array>
#include <cstdint>

namespace drivers
{
    class AccelerometerSensor
        : public RegisterSensor
    {
    public:
        using Acceleration = infra::Quantity<infra::MilliMeterPerSecondSquared, int32_t>;
        using Accelerometer = hal::Accelerometer<infra::MilliMeterPerSecondSquared, int32_t>;

        AccelerometerSensor(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin);

        Accelerometer& AsAccelerometer();

    protected:
        ~AccelerometerSensor() = default;

        void ClearMeasurementCallbacks() override;
        void DeliverAcceleration(infra::MemoryRange<const Acceleration> samples);
        bool AccelerometerRequested() const;

        static int16_t RawSample(const uint8_t* data);
        static Acceleration ToAcceleration(int32_t counts, int64_t microGPerCount);

    private:
        class AccelerometerAdapter
            : public Accelerometer
        {
        public:
            explicit AccelerometerAdapter(AccelerometerSensor& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            AccelerometerSensor& device;
        };

        AccelerometerAdapter accelerometerAdapter{ *this };
        infra::Function<void(Accelerometer::Samples)> onAccelerometerMeasurement;
    };
}

#endif
