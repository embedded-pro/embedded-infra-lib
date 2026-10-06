#ifndef BOARDS_BOOSTXL_K350QVG_S1_BOOSTXL_K350QVG_S1_SETUP_HPP
#define BOARDS_BOOSTXL_K350QVG_S1_BOOSTXL_K350QVG_S1_SETUP_HPP

#include "drivers/display/ssd2119/Ssd2119.hpp"
#include "drivers/display/ssd2119/Ssd2119BusAccessSpi.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include <array>

namespace boards
{
    // Panel tuning as written by TI's reference driver for the BOOSTXL-K350QVG-S1
    inline constexpr std::array<drivers::Ssd2119::Step, 7> boostxlK350qvgS1BeforeEntryMode{ {
        { 0x10, 0x0001, 0 },
        { 0x1e, 0x00ba, 0 },
        { 0x28, 0x0006, 0 },
        { 0x00, 0x0001, 0 },
        { 0x01, 0x30ef, 0 },
        { 0x02, 0x0600, 0 },
        { 0x10, 0x0000, 30 },
    } };

    inline constexpr std::array<drivers::Ssd2119::Step, 14> boostxlK350qvgS1AfterEntryMode{ {
        { 0x07, 0x0033, 0 },
        { 0x0c, 0x0005, 0 },
        { 0x30, 0x0000, 0 },
        { 0x31, 0x0400, 0 },
        { 0x32, 0x0106, 0 },
        { 0x33, 0x0700, 0 },
        { 0x34, 0x0002, 0 },
        { 0x35, 0x0702, 0 },
        { 0x36, 0x0707, 0 },
        { 0x37, 0x0203, 0 },
        { 0x3a, 0x1400, 0 },
        { 0x3b, 0x0f03, 0 },
        { 0x0d, 0x0007, 0 },
        { 0x0e, 0x3100, 0 },
    } };

    inline constexpr drivers::Ssd2119::Panel boostxlK350qvgS1Panel{ { 320, 240 }, true, true, boostxlK350qvgS1BeforeEntryMode, boostxlK350qvgS1AfterEntryMode };

    class BoostxlK350qvgS1Setup
    {
    public:
        BoostxlK350qvgS1Setup(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand, hal::GpioPin& reset, const infra::Function<void()>& onInitialized);
        BoostxlK350qvgS1Setup(const BoostxlK350qvgS1Setup& other) = delete;
        BoostxlK350qvgS1Setup& operator=(const BoostxlK350qvgS1Setup& other) = delete;

        hal::Display& Display();

    private:
        drivers::Ssd2119BusAccessSpi bus;
        drivers::Ssd2119 display;
    };
}

#endif
