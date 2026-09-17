#ifndef DRIVERS_IMU_LSM303DLHC_LSM303DLHC_HPP
#define DRIVERS_IMU_LSM303DLHC_LSM303DLHC_HPP

#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometer.hpp"
#include "drivers/imu/lsm303dlhc/Lsm303dlhcMagnetometer.hpp"

namespace drivers
{
    // Owns both halves of the part and sequences them. The cores are template parameters so a
    // buffered or polled stack composes without the facade knowing about the mixins, for instance
    // Lsm303dlhc<Lsm303dlhcAccelerometerWithFifo<Lsm303dlhcAccelerometer>, Lsm303dlhcWithPolling<Lsm303dlhcMagnetometer>>
    template<class AccelerometerCore = Lsm303dlhcAccelerometer, class MagnetometerCore = Lsm303dlhcMagnetometer>
    class Lsm303dlhc
        : public services::Stoppable
    {
    public:
        using Accelerometer = typename AccelerometerCore::Accelerometer;
        using Magnetometer = typename MagnetometerCore::Magnetometer;
        using Temperature = typename MagnetometerCore::Temperature;

        enum class InitializationResult : uint8_t
        {
            success,
            accelerometerNotFound,
            magnetometerNotFound
        };

        struct Config
        {
            typename AccelerometerCore::Config accelerometer;
            typename MagnetometerCore::Config magnetometer;
        };

        Lsm303dlhc(services::RegisterBusAccess& accelerometerBus, services::RegisterBusAccess& magnetometerBus,
            hal::GpioPin& accelerometerDataReadyPin = hal::dummyPin, hal::GpioPin& magnetometerDataReadyPin = hal::dummyPin);
        Lsm303dlhc(const Lsm303dlhc& other) = delete;
        Lsm303dlhc& operator=(const Lsm303dlhc& other) = delete;

        // Implementation of services::Stoppable
        void Stop(const infra::Function<void()>& onDone) override;

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);
        void MeasureTemperature(const infra::Function<void(Temperature)>& onDone);

        Accelerometer& AsAccelerometer();
        Magnetometer& AsMagnetometer();

        AccelerometerCore& AccelerometerDevice();
        MagnetometerCore& MagnetometerDevice();

    private:
        void InitializeMagnetometer();

        AccelerometerCore accelerometer;
        MagnetometerCore magnetometer;

        Config config;
        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onStopped;
    };

    ////    Implementation    ////

    template<class AccelerometerCore, class MagnetometerCore>
    Lsm303dlhc<AccelerometerCore, MagnetometerCore>::Lsm303dlhc(services::RegisterBusAccess& accelerometerBus, services::RegisterBusAccess& magnetometerBus,
        hal::GpioPin& accelerometerDataReadyPin, hal::GpioPin& magnetometerDataReadyPin)
        : accelerometer(accelerometerBus, accelerometerDataReadyPin)
        , magnetometer(magnetometerBus, magnetometerDataReadyPin)
    {}

    template<class AccelerometerCore, class MagnetometerCore>
    void Lsm303dlhc<AccelerometerCore, MagnetometerCore>::Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone)
    {
        this->config = config;
        onInitialized = onDone;

        accelerometer.Initialize(this->config.accelerometer, [this](typename AccelerometerCore::InitializationResult result)
            {
                if (result != AccelerometerCore::InitializationResult::success)
                    return onInitialized(InitializationResult::accelerometerNotFound);

                InitializeMagnetometer();
            });
    }

    template<class AccelerometerCore, class MagnetometerCore>
    void Lsm303dlhc<AccelerometerCore, MagnetometerCore>::InitializeMagnetometer()
    {
        magnetometer.Initialize(config.magnetometer, [this](typename MagnetometerCore::InitializationResult result)
            {
                onInitialized(result == MagnetometerCore::InitializationResult::success ? InitializationResult::success : InitializationResult::magnetometerNotFound);
            });
    }

    template<class AccelerometerCore, class MagnetometerCore>
    void Lsm303dlhc<AccelerometerCore, MagnetometerCore>::Stop(const infra::Function<void()>& onDone)
    {
        onStopped = onDone;

        accelerometer.Stop([this]()
            {
                magnetometer.Stop([this]()
                    {
                        onStopped();
                    });
            });
    }

    template<class AccelerometerCore, class MagnetometerCore>
    void Lsm303dlhc<AccelerometerCore, MagnetometerCore>::MeasureTemperature(const infra::Function<void(Temperature)>& onDone)
    {
        magnetometer.MeasureTemperature(onDone);
    }

    template<class AccelerometerCore, class MagnetometerCore>
    typename Lsm303dlhc<AccelerometerCore, MagnetometerCore>::Accelerometer& Lsm303dlhc<AccelerometerCore, MagnetometerCore>::AsAccelerometer()
    {
        return accelerometer.AsAccelerometer();
    }

    template<class AccelerometerCore, class MagnetometerCore>
    typename Lsm303dlhc<AccelerometerCore, MagnetometerCore>::Magnetometer& Lsm303dlhc<AccelerometerCore, MagnetometerCore>::AsMagnetometer()
    {
        return magnetometer.AsMagnetometer();
    }

    template<class AccelerometerCore, class MagnetometerCore>
    AccelerometerCore& Lsm303dlhc<AccelerometerCore, MagnetometerCore>::AccelerometerDevice()
    {
        return accelerometer;
    }

    template<class AccelerometerCore, class MagnetometerCore>
    MagnetometerCore& Lsm303dlhc<AccelerometerCore, MagnetometerCore>::MagnetometerDevice()
    {
        return magnetometer;
    }
}

#endif
