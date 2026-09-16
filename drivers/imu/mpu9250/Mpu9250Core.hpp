#ifndef DRIVERS_IMU_MPU9250_MPU9250_CORE_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_CORE_HPP

#include "drivers/imu/mpu9250/Mpu9250BusAccess.hpp"
#include "hal/interfaces/Accelerometer.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Gyroscope.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Sequencer.hpp"
#include "infra/util/Unit.hpp"
#include <array>
#include <cstdint>

namespace drivers
{
    class Mpu9250Core
    {
    public:
        using Acceleration = infra::Quantity<infra::MilliMeterPerSecondSquared, int32_t>;
        using AngularVelocity = infra::Quantity<infra::MilliDegreePerSecond, int32_t>;
        using Temperature = infra::Quantity<infra::MilliCelsius, int32_t>;

        using Accelerometer = hal::Accelerometer<infra::MilliMeterPerSecondSquared, int32_t>;
        using Gyroscope = hal::Gyroscope<infra::MilliDegreePerSecond, int32_t>;

        enum class AccelerometerFullScale : uint8_t
        {
            g2 = 0,
            g4 = 1,
            g8 = 2,
            g16 = 3
        };

        enum class GyroscopeFullScale : uint8_t
        {
            dps250 = 0,
            dps500 = 1,
            dps1000 = 2,
            dps2000 = 3
        };

        enum class GyroscopeLowPassFilter : uint8_t
        {
            bandwidth250Hz = 0,
            bandwidth184Hz = 1,
            bandwidth92Hz = 2,
            bandwidth41Hz = 3,
            bandwidth20Hz = 4,
            bandwidth10Hz = 5,
            bandwidth5Hz = 6,
            bandwidth3600Hz = 7
        };

        enum class AccelerometerLowPassFilter : uint8_t
        {
            bandwidth218Hz = 1,
            bandwidth99Hz = 2,
            bandwidth45Hz = 3,
            bandwidth21Hz = 4,
            bandwidth10Hz = 5,
            bandwidth5Hz = 6,
            bandwidth420Hz = 7
        };

        enum class ClockSource : uint8_t
        {
            internalOscillator = 0,
            automatic = 1,
            stopped = 7
        };

        enum class PowerMode : uint8_t
        {
            normal,
            standby,
            sleep
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh = 0,
            activeLow = 1
        };

        enum class InterruptDrive : uint8_t
        {
            pushPull = 0,
            openDrain = 1
        };

        enum class InterruptLatch : uint8_t
        {
            pulsed = 0,
            latchedUntilCleared = 1
        };

        enum class InitializationResult : uint8_t
        {
            success,
            deviceNotFound
        };

        struct Config
        {
            AccelerometerFullScale accelerometerFullScale = AccelerometerFullScale::g2;
            GyroscopeFullScale gyroscopeFullScale = GyroscopeFullScale::dps250;
            GyroscopeLowPassFilter gyroscopeLowPassFilter = GyroscopeLowPassFilter::bandwidth41Hz;
            AccelerometerLowPassFilter accelerometerLowPassFilter = AccelerometerLowPassFilter::bandwidth45Hz;
            ClockSource clockSource = ClockSource::automatic;
            uint8_t sampleRateDivider = 4;

            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
            InterruptDrive interruptDrive = InterruptDrive::pushPull;
            InterruptLatch interruptLatch = InterruptLatch::pulsed;
            bool clearInterruptOnAnyRead = true;

            uint8_t expectedWhoAmI = 0x71;
        };

        explicit Mpu9250Core(Mpu9250BusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);
        Mpu9250Core(const Mpu9250Core& other) = delete;
        Mpu9250Core& operator=(const Mpu9250Core& other) = delete;

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);
        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetAccelerometerFullScale(AccelerometerFullScale scale, const infra::Function<void()>& onDone);
        void SetGyroscopeFullScale(GyroscopeFullScale scale, const infra::Function<void()>& onDone);

        void MeasureTemperature(const infra::Function<void(Temperature)>& onDone);

        Accelerometer& AsAccelerometer();
        Gyroscope& AsGyroscope();

    protected:
        static constexpr uint8_t registerSelfTestXGyroscope = 0x00;
        static constexpr uint8_t registerSelfTestXAccelerometer = 0x0d;
        static constexpr uint8_t registerSampleRateDivider = 0x19;
        static constexpr uint8_t registerConfiguration = 0x1a;
        static constexpr uint8_t registerGyroscopeConfig = 0x1b;
        static constexpr uint8_t registerAccelerometerConfig = 0x1c;
        static constexpr uint8_t registerAccelerometerConfig2 = 0x1d;
        static constexpr uint8_t registerLowPowerAccelerometerOutputDataRate = 0x1e;
        static constexpr uint8_t registerWakeOnMotionThreshold = 0x1f;
        static constexpr uint8_t registerFifoEnable = 0x23;
        static constexpr uint8_t registerInterruptPinConfig = 0x37;
        static constexpr uint8_t registerInterruptEnable = 0x38;
        static constexpr uint8_t registerInterruptStatus = 0x3a;
        static constexpr uint8_t registerAccelerometerXOutHigh = 0x3b;
        static constexpr uint8_t registerTemperatureOutHigh = 0x41;
        static constexpr uint8_t registerGyroscopeXOutHigh = 0x43;
        static constexpr uint8_t registerMotionDetectControl = 0x69;
        static constexpr uint8_t registerUserControl = 0x6a;
        static constexpr uint8_t registerPowerManagement1 = 0x6b;
        static constexpr uint8_t registerPowerManagement2 = 0x6c;
        static constexpr uint8_t registerFifoCountHigh = 0x72;
        static constexpr uint8_t registerFifoReadWrite = 0x74;
        static constexpr uint8_t registerWhoAmI = 0x75;

        static constexpr uint8_t deviceReset = 0x80;
        static constexpr uint8_t sleepEnable = 0x40;
        static constexpr uint8_t cycleEnable = 0x20;
        static constexpr uint8_t gyroscopeStandby = 0x10;
        static constexpr uint8_t i2cInterfaceDisable = 0x10;
        static constexpr uint8_t allAxesDisabled = 0x3f;
        static constexpr uint8_t rawDataReadyInterrupt = 0x01;
        static constexpr uint8_t fifoOverflowInterrupt = 0x10;
        static constexpr uint8_t wakeOnMotionInterrupt = 0x40;

        static constexpr std::size_t measurementSize = 14;

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone);
        void WriteRegister(uint8_t address, uint8_t value, const infra::Function<void()>& onDone);
        void ModifyRegister(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone);

        virtual void StartSampling(const infra::Function<void()>& onSampleAvailable);
        virtual void StopSampling();
        virtual void ReadAndDeliverSamples();

        void DeliverAcceleration(infra::MemoryRange<const Acceleration> samples);
        void DeliverAngularVelocity(infra::MemoryRange<const AngularVelocity> samples);

        static int16_t RawSample(const uint8_t* data);
        Acceleration ToAcceleration(int16_t raw) const;
        AngularVelocity ToAngularVelocity(int16_t raw) const;
        static Temperature ToTemperature(int16_t raw);

        bool AccelerometerRequested() const;
        bool GyroscopeRequested() const;
        bool Sampling() const;

        hal::InterruptTrigger DataReadyTrigger() const;

        Mpu9250BusAccess& bus;
        hal::InputPin dataReadyPin;
        bool dataReadyPinConnected;
        infra::Sequencer sequencer;
        infra::TimerSingleShot delayTimer;
        Config config;

    private:
        class AccelerometerAdapter
            : public Accelerometer
        {
        public:
            explicit AccelerometerAdapter(Mpu9250Core& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            Mpu9250Core& device;
        };

        class GyroscopeAdapter
            : public Gyroscope
        {
        public:
            explicit GyroscopeAdapter(Mpu9250Core& device);

            void Start(const infra::Function<void(Samples)>& onMeasurement) override;
            void Stop() override;

        private:
            Mpu9250Core& device;
        };

        void ConfigurationSteps();
        void UpdateSampling();
        uint8_t InterruptPinConfigValue() const;
        uint8_t PowerManagement1Value(PowerMode mode) const;
        static uint8_t PowerManagement2Value(PowerMode mode);

        AccelerometerAdapter accelerometerAdapter{ *this };
        GyroscopeAdapter gyroscopeAdapter{ *this };

        infra::Function<void(Accelerometer::Samples)> onAccelerometerMeasurement;
        infra::Function<void(Gyroscope::Samples)> onGyroscopeMeasurement;

        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onPowerModeSet;
        infra::AutoResetFunction<void(Temperature)> onTemperature;
        infra::AutoResetFunction<void()> onModified;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<Acceleration, 3> accelerationSamples = {};
        std::array<AngularVelocity, 3> angularVelocitySamples = {};
        std::array<uint8_t, 2> temperatureBuffer = {};

        uint8_t scratch = 0;
        uint8_t modifyValue = 0;
        uint8_t modifyAddress = 0;
        uint8_t modifyClearMask = 0;
        uint8_t modifySetMask = 0;
        uint8_t writeValue = 0;
        PowerMode requestedPowerMode = PowerMode::sleep;
        bool waitForStartUp = false;
        PowerMode powerMode = PowerMode::sleep;
        bool initialized = false;
        bool sampling = false;
    };
}

#endif
