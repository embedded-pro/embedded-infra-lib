#ifndef DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace drivers
{
    class Mpu9250BusAccess
    {
    public:
        Mpu9250BusAccess() = default;
        Mpu9250BusAccess(const Mpu9250BusAccess& other) = delete;
        Mpu9250BusAccess& operator=(const Mpu9250BusAccess& other) = delete;

    protected:
        ~Mpu9250BusAccess() = default;

    public:
        virtual void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) = 0;
        virtual void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;

        virtual bool RequiresI2cSlaveInterfaceDisabled() const = 0;
    };
}

#endif
