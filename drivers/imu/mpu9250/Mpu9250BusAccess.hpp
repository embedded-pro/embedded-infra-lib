#ifndef DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_HPP

#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    class Mpu9250BusAccess
        : public services::RegisterBusAccess
    {
    protected:
        ~Mpu9250BusAccess() = default;

    public:
        virtual bool RequiresI2cSlaveInterfaceDisabled() const = 0;
    };
}

#endif
