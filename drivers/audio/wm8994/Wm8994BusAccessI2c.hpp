#ifndef DRIVERS_AUDIO_WM8994_WM8994_BUS_ACCESS_I2C_HPP
#define DRIVERS_AUDIO_WM8994_WM8994_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    class Wm8994BusAccessI2c
        : public services::RegisterBusAccessHalfWord
    {
    public:
        static constexpr hal::I2cAddress deviceAddress{ 0x1a };

        explicit Wm8994BusAccessI2c(hal::I2cMaster& i2c);

        void ReadRegister(uint16_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint16_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        hal::I2cMasterRegisterAccessHalfWordBigEndian registerAccess;
    };
}

#endif
