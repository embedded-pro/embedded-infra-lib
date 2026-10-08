#ifndef DRIVERS_IMU_LIS302DL_LIS302DL_CORE_HPP
#define DRIVERS_IMU_LIS302DL_LIS302DL_CORE_HPP

#include "drivers/imu/common/AccelerometerSensor.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace drivers
{
    class Lis302dlCore
        : public AccelerometerSensor
    {
    public:
        // Contract used by SensorWithPolling
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t dataAvailable = 0x08;

        enum class OutputDataRate : uint8_t
        {
            hertz100,
            hertz400
        };

        enum class FullScale : uint8_t
        {
            g2,
            g8
        };

        enum class PowerMode : uint8_t
        {
            normal,
            powerDown
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh,
            activeLow
        };

        struct Config
        {
            OutputDataRate outputDataRate = OutputDataRate::hertz100;
            FullScale fullScale = FullScale::g2;
            bool enableX = true;
            bool enableY = true;
            bool enableZ = true;
            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
        };

        explicit Lis302dlCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);

        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetFullScale(FullScale scale, const infra::Function<void()>& onDone);

    protected:
        void ReadAndDeliverSamples() override;
        void EnableDataReadyInterrupt(bool enable) override;
        hal::InterruptTrigger DataReadyTrigger() const override;

    private:
        void VerifyIdentification();
        void CompleteInitialization();
        void DeliverMeasurement();

        uint8_t Control1Value(PowerMode mode) const;
        uint8_t Control3Value() const;
        static int64_t MicroGPerCount(FullScale scale);

        static constexpr uint8_t registerWhoAmI = 0x0f;
        static constexpr uint8_t registerControl1 = 0x20;
        static constexpr uint8_t registerControl2 = 0x21;
        static constexpr uint8_t registerControl3 = 0x22;
        static constexpr uint8_t registerOutX = 0x29;
        static constexpr uint8_t identification = 0x3b;

        static constexpr uint8_t dataRate400 = 0x80;
        static constexpr uint8_t deviceActive = 0x40;
        static constexpr uint8_t fullScale8g = 0x20;
        static constexpr uint8_t axisEnableX = 0x01;
        static constexpr uint8_t axisEnableY = 0x02;
        static constexpr uint8_t axisEnableZ = 0x04;
        static constexpr uint8_t boot = 0x40;
        static constexpr uint8_t interruptActiveLow = 0x80;
        static constexpr uint8_t dataReadyOnInterrupt1 = 0x04;
        static constexpr uint8_t interrupt1ConfigurationMask = 0x07;

        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;

        static constexpr std::chrono::milliseconds bootDelay{ 10 };

        Config config;
        PowerMode powerMode = PowerMode::powerDown;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<Acceleration, axisCount> accelerationSamples = {};

        uint8_t scratch = 0;

        infra::AutoResetFunction<void(InitializationResult)> onInitialized;
        infra::AutoResetFunction<void()> onSequenceDone;
    };
}

#endif
