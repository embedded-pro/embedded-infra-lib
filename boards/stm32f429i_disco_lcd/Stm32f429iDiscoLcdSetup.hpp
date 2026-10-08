#ifndef BOARDS_STM32F429I_DISCO_LCD_STM32F429I_DISCO_LCD_SETUP_HPP
#define BOARDS_STM32F429I_DISCO_LCD_STM32F429I_DISCO_LCD_SETUP_HPP

#include "drivers/display/ili9341/Ili9341.hpp"
#include "drivers/display/ili9341/Ili9341BusAccessSpi.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include "services/peripheral/SpiMasterWithChipSelect.hpp"
#include <array>
#include <cstdint>

namespace boards
{
    inline constexpr std::array<uint8_t, 0> stm32f429iDiscoLcdNoParameters{};
    inline constexpr std::array<uint8_t, 3> stm32f429iDiscoLcdUndocumentedCa{ 0xc3, 0x08, 0x50 };
    inline constexpr std::array<uint8_t, 3> stm32f429iDiscoLcdPowerControlB{ 0x00, 0xc1, 0x30 };
    inline constexpr std::array<uint8_t, 4> stm32f429iDiscoLcdPowerOnSequence{ 0x64, 0x03, 0x12, 0x81 };
    inline constexpr std::array<uint8_t, 3> stm32f429iDiscoLcdDriverTimingControlA{ 0x85, 0x00, 0x78 };
    inline constexpr std::array<uint8_t, 5> stm32f429iDiscoLcdPowerControlA{ 0x39, 0x2c, 0x00, 0x34, 0x02 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdPumpRatioControl{ 0x20 };
    inline constexpr std::array<uint8_t, 2> stm32f429iDiscoLcdDriverTimingControlB{ 0x00, 0x00 };
    inline constexpr std::array<uint8_t, 2> stm32f429iDiscoLcdFrameRateControl{ 0x00, 0x1b };
    inline constexpr std::array<uint8_t, 2> stm32f429iDiscoLcdDisplayFunctionControl{ 0x0a, 0xa2 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdPowerControl1{ 0x10 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdPowerControl2{ 0x10 };
    inline constexpr std::array<uint8_t, 2> stm32f429iDiscoLcdVcomControl1{ 0x45, 0x15 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdVcomControl2{ 0x90 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdMemoryAccessControl{ 0xc8 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdThreeGammaControl{ 0x00 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdRgbInterfaceSignalControl{ 0xc2 };
    inline constexpr std::array<uint8_t, 4> stm32f429iDiscoLcdDisplayFunctionControlForRgb{ 0x0a, 0xa7, 0x27, 0x04 };
    inline constexpr std::array<uint8_t, 4> stm32f429iDiscoLcdColumnAddress{ 0x00, 0x00, 0x00, 0xef };
    inline constexpr std::array<uint8_t, 4> stm32f429iDiscoLcdPageAddress{ 0x00, 0x00, 0x01, 0x3f };
    inline constexpr std::array<uint8_t, 3> stm32f429iDiscoLcdInterfaceControl{ 0x01, 0x00, 0x06 };
    inline constexpr std::array<uint8_t, 1> stm32f429iDiscoLcdGammaSet{ 0x01 };
    inline constexpr std::array<uint8_t, 15> stm32f429iDiscoLcdPositiveGamma{ 0x0f, 0x29, 0x24, 0x0c, 0x0e, 0x09, 0x4e, 0x78, 0x3c, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00 };
    inline constexpr std::array<uint8_t, 15> stm32f429iDiscoLcdNegativeGamma{ 0x00, 0x16, 0x1b, 0x04, 0x11, 0x07, 0x31, 0x33, 0x42, 0x05, 0x0c, 0x0a, 0x28, 0x2f, 0x0f };

    inline constexpr std::array<drivers::Ili9341::Command, 27> stm32f429iDiscoLcdCommands{ {
        { 0xca, stm32f429iDiscoLcdUndocumentedCa, 0 },
        { 0xcf, stm32f429iDiscoLcdPowerControlB, 0 },
        { 0xed, stm32f429iDiscoLcdPowerOnSequence, 0 },
        { 0xe8, stm32f429iDiscoLcdDriverTimingControlA, 0 },
        { 0xcb, stm32f429iDiscoLcdPowerControlA, 0 },
        { 0xf7, stm32f429iDiscoLcdPumpRatioControl, 0 },
        { 0xea, stm32f429iDiscoLcdDriverTimingControlB, 0 },
        { 0xb1, stm32f429iDiscoLcdFrameRateControl, 0 },
        { 0xb6, stm32f429iDiscoLcdDisplayFunctionControl, 0 },
        { 0xc0, stm32f429iDiscoLcdPowerControl1, 0 },
        { 0xc1, stm32f429iDiscoLcdPowerControl2, 0 },
        { 0xc5, stm32f429iDiscoLcdVcomControl1, 0 },
        { 0xc7, stm32f429iDiscoLcdVcomControl2, 0 },
        { 0x36, stm32f429iDiscoLcdMemoryAccessControl, 0 },
        { 0xf2, stm32f429iDiscoLcdThreeGammaControl, 0 },
        { 0xb0, stm32f429iDiscoLcdRgbInterfaceSignalControl, 0 },
        { 0xb6, stm32f429iDiscoLcdDisplayFunctionControlForRgb, 0 },
        { 0x2a, stm32f429iDiscoLcdColumnAddress, 0 },
        { 0x2b, stm32f429iDiscoLcdPageAddress, 0 },
        { 0xf6, stm32f429iDiscoLcdInterfaceControl, 0 },
        { 0x2c, stm32f429iDiscoLcdNoParameters, 200 },
        { 0x26, stm32f429iDiscoLcdGammaSet, 0 },
        { 0xe0, stm32f429iDiscoLcdPositiveGamma, 0 },
        { 0xe1, stm32f429iDiscoLcdNegativeGamma, 0 },
        { 0x11, stm32f429iDiscoLcdNoParameters, 200 },
        { 0x29, stm32f429iDiscoLcdNoParameters, 0 },
        { 0x2c, stm32f429iDiscoLcdNoParameters, 0 },
    } };

    inline constexpr drivers::Ili9341::Panel stm32f429iDiscoLcdPanel{ stm32f429iDiscoLcdCommands };

    class Stm32f429iDiscoLcdSetup
    {
    public:
        Stm32f429iDiscoLcdSetup(hal::SpiMaster& spi, hal::GpioPin& chipSelect, hal::GpioPin& dataCommand, const infra::Function<void()>& onInitialized);
        Stm32f429iDiscoLcdSetup(const Stm32f429iDiscoLcdSetup& other) = delete;
        Stm32f429iDiscoLcdSetup& operator=(const Stm32f429iDiscoLcdSetup& other) = delete;

    private:
        services::SpiMasterWithChipSelect spiWithChipSelect;
        drivers::Ili9341BusAccessSpi bus;
        drivers::Ili9341 controller;
    };
}

#endif
