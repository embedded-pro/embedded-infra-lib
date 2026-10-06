#ifndef DRIVERS_DISPLAY_SSD2119_SSD2119_BOOSTXL_K350QVG_S1_HPP
#define DRIVERS_DISPLAY_SSD2119_SSD2119_BOOSTXL_K350QVG_S1_HPP

#include "drivers/display/ssd2119/Ssd2119.hpp"
#include "drivers/display/ssd2119/Ssd2119BusAccessSpi.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include <array>

namespace drivers
{
    // The 3.5" Kentec K350QVG-V2-F panel on the BOOSTXL-K350QVG-S1 BoosterPack, in the configuration in which the
    // BoosterPack ships: 4-wire, 8-bit SPI with a data/command line (LCD_SDC, data when high) and an active low
    // reset (LCD_RESET). Configure the SPI master for mode 0, 8-bit frames, most significant bit first and at
    // most 15 MHz, and wrap it in a services::SpiMasterWithChipSelect that drives LCD_SCS.
    //
    // The backlight is a separate boost converter that is switched with the LED PWM pin, so it stays with the
    // application. The frame memory is not cleared by the bring-up: paint the whole screen before switching
    // the backlight on.
    //
    // The register values are the ones that Texas Instruments' reference driver for this BoosterPack writes,
    // which tune the power supply, the common voltage and the gamma curve to this panel. The orientation is that
    // reference driver's landscape one, with the flexible connector at the bottom.
    inline constexpr std::array<Ssd2119::Step, 7> boostxlK350qvgS1BeforeEntryMode{ {
        { 0x10, 0x0001, 0 },
        { 0x1e, 0x00ba, 0 },
        { 0x28, 0x0006, 0 },
        { 0x00, 0x0001, 0 },
        { 0x01, 0x30ef, 0 },
        { 0x02, 0x0600, 0 },
        { 0x10, 0x0000, 30 },
    } };

    inline constexpr std::array<Ssd2119::Step, 14> boostxlK350qvgS1AfterEntryMode{ {
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

    inline constexpr Ssd2119::Panel boostxlK350qvgS1Panel{ { 320, 240 }, true, true, boostxlK350qvgS1BeforeEntryMode, boostxlK350qvgS1AfterEntryMode };

    class BoostxlK350qvgS1
    {
    public:
        BoostxlK350qvgS1(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand, hal::GpioPin& reset, const infra::Function<void()>& onInitialized);
        BoostxlK350qvgS1(const BoostxlK350qvgS1& other) = delete;
        BoostxlK350qvgS1& operator=(const BoostxlK350qvgS1& other) = delete;

        hal::Display& Display();

    private:
        Ssd2119BusAccessSpi bus;
        Ssd2119 display;
    };
}

#endif
