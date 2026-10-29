#ifndef DRIVERS_EEPROM_EEPROM24XX_HPP
#define DRIVERS_EEPROM_EEPROM24XX_HPP

#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/I2c.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace drivers
{
    // 24xx01 to 24xx16: one word-address byte, and the address bits above it select a 256-byte block in the device
    // address. A page the device does not acknowledge within the write timeout is skipped and a read it does not
    // acknowledge returns erased bytes, so every operation completes and a caller that verifies its data detects a
    // missing or failing device.
    class Eeprom24xx
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t blockSize{ 256 };
        static constexpr uint32_t maxPageSize{ 16 };
        static constexpr uint32_t maxSize{ 8 * blockSize };

        struct Config
        {
            uint8_t deviceAddress{ 0x50 };
            uint32_t size{ 512 };
            uint32_t pageSize{ 16 };

            // Longer than the device's maximum write cycle, during which it acknowledges nothing
            infra::Duration writeTimeout{ std::chrono::milliseconds(10) };
        };

        Eeprom24xx(hal::I2cMaster& master, const Config& config);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        hal::I2cAddress DeviceAddress(uint32_t address) const;
        void Start(const infra::Function<void()>& onDone);
        void Finish();
        void StartWrite(uint32_t address, uint32_t size);
        void WriteNextPage();
        void SendPage();
        void PageSent(hal::Result result);
        void Poll();
        void Polled(hal::Result result);
        void PageDone();
        infra::ByteRange CurrentChunk() const;
        void ReadNextBlock();
        void WordAddressSent(hal::Result result);
        void ChunkReceived(hal::Result result);

    private:
        hal::I2cMaster& master;
        Config config;
        infra::ConstByteRange source;
        infra::ByteRange destination;
        bool erasing{ false };
        bool busy{ false };
        bool expired{ false };
        uint32_t address{ 0 };
        uint32_t remaining{ 0 };
        uint32_t chunk{ 0 };
        std::array<uint8_t, 1 + maxPageSize> frame{};
        infra::TimerSingleShot writeDeadline;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
