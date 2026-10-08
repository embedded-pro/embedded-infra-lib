#ifndef DRIVERS_IMU_LIS3DSH_LIS3DSH_CORE_HPP
#define DRIVERS_IMU_LIS3DSH_LIS3DSH_CORE_HPP

#include "drivers/imu/common/AccelerometerSensor.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <cstdint>

namespace drivers
{
    class Lis3dshCore
        : public AccelerometerSensor
    {
    public:
        // Contract used by SensorWithPolling
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t dataAvailable = 0x08;

        enum class OutputDataRate : uint8_t
        {
            millihertz3125 = 1,
            millihertz6250 = 2,
            millihertz12500 = 3,
            hertz25 = 4,
            hertz50 = 5,
            hertz100 = 6,
            hertz400 = 7,
            hertz800 = 8,
            hertz1600 = 9
        };

        enum class FullScale : uint8_t
        {
            g2 = 0,
            g4 = 1,
            g6 = 2,
            g8 = 3,
            g16 = 4
        };

        enum class Bandwidth : uint8_t
        {
            hertz800 = 0,
            hertz400 = 1,
            hertz200 = 2,
            hertz50 = 3
        };

        enum class InterruptPolarity : uint8_t
        {
            activeHigh,
            activeLow
        };

        enum class PowerMode : uint8_t
        {
            powerDown,
            normal
        };

        struct Config
        {
            OutputDataRate outputDataRate = OutputDataRate::hertz100;
            FullScale fullScale = FullScale::g2;
            Bandwidth bandwidth = Bandwidth::hertz800;
            bool blockDataUpdate = true;
            bool enableX = true;
            bool enableY = true;
            bool enableZ = true;
            InterruptPolarity interruptPolarity = InterruptPolarity::activeHigh;
        };

        explicit Lis3dshCore(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin);

        void Initialize(const Config& config, const infra::Function<void(InitializationResult)>& onDone);

        void SetPowerMode(PowerMode mode, const infra::Function<void()>& onDone);
        PowerMode CurrentPowerMode() const;

        void SetOutputDataRate(OutputDataRate rate, const infra::Function<void()>& onDone);
        void SetFullScale(FullScale scale, const infra::Function<void()>& onDone);
        void SetBandwidth(Bandwidth bandwidth, const infra::Function<void()>& onDone);

    protected:
        void ReadAndDeliverSamples() override;
        void EnableDataReadyInterrupt(bool enable) override;
        hal::InterruptTrigger DataReadyTrigger() const override;

    private:
        void VerifyIdentification();
        void CompleteInitialization();
        void DeliverMeasurement();

        uint8_t Control3Value() const;
        uint8_t Control4Value(PowerMode mode) const;
        uint8_t Control5Value() const;
        static int64_t MicroGPerCount(FullScale scale);

        static constexpr uint8_t registerWhoAmI = 0x0f;
        static constexpr uint8_t registerControl3 = 0x23;
        static constexpr uint8_t registerControl4 = 0x20;
        static constexpr uint8_t registerControl5 = 0x24;
        static constexpr uint8_t registerControl6 = 0x25;
        static constexpr uint8_t registerOutXLow = 0x28;
        static constexpr uint8_t identification = 0x3f;

        static constexpr uint8_t dataReadyEnable = 0x80;
        static constexpr uint8_t interruptActiveHigh = 0x40;
        static constexpr uint8_t blockDataUpdateEnable = 0x08;
        static constexpr uint8_t axisEnableX = 0x01;
        static constexpr uint8_t axisEnableY = 0x02;
        static constexpr uint8_t axisEnableZ = 0x04;
        static constexpr uint8_t boot = 0x80;
        static constexpr uint8_t addressIncrement = 0x10;

        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;

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
