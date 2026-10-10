#ifndef SERVICES_I2C_SCANNER_HPP
#define SERVICES_I2C_SCANNER_HPP

#include "hal/interfaces/I2c.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class I2cScanner
    {
    public:
        explicit I2cScanner(hal::I2cMaster& i2c);

        void Scan(const infra::Function<void(hal::I2cAddress address)>& onDeviceFound, const infra::Function<void(uint32_t numberOfDevices)>& onDone);
        bool Scanning() const;

    private:
        static constexpr uint16_t firstAddress = 0x08;
        static constexpr uint16_t lastAddress = 0x77;

        void Probe();
        void Probed(hal::Result result);
        void Finish();

    private:
        hal::I2cMaster& i2c;
        infra::Function<void(hal::I2cAddress address)> onDeviceFound;
        infra::AutoResetFunction<void(uint32_t numberOfDevices)> onDone;
        std::array<uint8_t, 1> data{};
        uint16_t address{ firstAddress };
        uint32_t numberOfDevices{ 0 };
        bool scanning{ false };
    };
}

#endif
