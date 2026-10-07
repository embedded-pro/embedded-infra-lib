#ifndef DRIVERS_DISPLAY_ILI9341_ILI9341_HPP
#define DRIVERS_DISPLAY_ILI9341_ILI9341_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class Ili9341
    {
    public:
        struct Command
        {
            uint8_t command;
            infra::ConstByteRange parameters;
            uint16_t delayAfterInMilliseconds;
        };

        struct Panel
        {
            infra::MemoryRange<const Command> commands;
        };

        Ili9341(services::RegisterBusAccess& bus, const Panel& panel, const infra::Function<void()>& onInitialized);
        Ili9341(const Ili9341& other) = delete;
        Ili9341& operator=(const Ili9341& other) = delete;
        ~Ili9341() = default;

    private:
        void WriteNextCommand();
        void CommandWritten();

    private:
        services::RegisterBusAccess& bus;
        Panel panel;
        infra::TimerSingleShot timer;
        infra::AutoResetFunction<void()> initialized;
        std::size_t commandIndex{ 0 };
    };
}

#endif
