#ifndef DRIVERS_IMU_COMMON_TEST_IMU_TEST_SENSOR_HPP
#define DRIVERS_IMU_COMMON_TEST_IMU_TEST_SENSOR_HPP

#include "drivers/imu/common/AccelerometerSensor.hpp"
#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstdint>

namespace drivers
{
    class ImuTestSensor
        : public AccelerometerSensor
    {
    public:
        static constexpr uint8_t registerStatus = 0x27;
        static constexpr uint8_t dataAvailable = 0x08;
        static constexpr uint8_t registerOutXLow = 0x28;
        static constexpr uint8_t registerControl = 0x20;
        static constexpr uint8_t dataReadyBit = 0x04;
        static constexpr int64_t microGPerCount = 61;

        explicit ImuTestSensor(services::RegisterBusAccess& bus, hal::GpioPin& dataReadyPin = hal::dummyPin)
            : AccelerometerSensor(bus, dataReadyPin)
        {}

        void Initialize()
        {
            initialized = true;
        }

        void TestRead(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
        {
            ReadRegister(address, data, onDone);
        }

        void TestWrite(uint8_t address, uint8_t value, const infra::Function<void()>& onDone)
        {
            WriteRegister(address, value, onDone);
        }

        void TestModify(uint8_t address, uint8_t clearMask, uint8_t setMask, const infra::Function<void()>& onDone)
        {
            ModifyRegister(address, clearMask, setMask, onDone);
        }

        void TestRunWriteSequence(uint8_t address, uint8_t value, const infra::Function<void()>& onDone)
        {
            runner.Clear();
            runner.Push(services::RegisterStepRunner::WriteRegister{ address, value });
            runner.Start(onDone);
        }

        bool TestSampling() const
        {
            return Sampling();
        }

        static Acceleration TestToAcceleration(int32_t counts, int64_t microGPerCount)
        {
            return ToAcceleration(counts, microGPerCount);
        }

    protected:
        void ReadAndDeliverSamples() override
        {
            ReadRegister(registerOutXLow, infra::MakeRange(measurementBuffer), [self = KeepAlive(*this)]()
                {
                    for (std::size_t i = 0; i != axisCount; ++i)
                        self->accelerationSamples[i] = ToAcceleration(RawSample(&self->measurementBuffer[2 * i]), microGPerCount);
                    self->DeliverAcceleration(infra::MakeRange(self->accelerationSamples));
                });
        }

        void EnableDataReadyInterrupt(bool enable) override
        {
            ModifyRegister(registerControl, dataReadyBit, enable ? dataReadyBit : uint8_t{ 0 }, infra::emptyFunction);
        }

    private:
        static constexpr std::size_t measurementSize = 6;
        static constexpr std::size_t axisCount = 3;

        std::array<uint8_t, measurementSize> measurementBuffer = {};
        std::array<Acceleration, axisCount> accelerationSamples = {};
    };
}

#endif
