#ifndef DRIVERS_DISPLAY_MIPI_DSI_MIPI_DCS_HPP
#define DRIVERS_DISPLAY_MIPI_DSI_MIPI_DCS_HPP

#include <cstdint>

namespace drivers
{
    namespace dcs
    {
        constexpr uint8_t softReset = 0x01;
        constexpr uint8_t readDisplayId = 0x04;
        constexpr uint8_t enterSleepMode = 0x10;
        constexpr uint8_t exitSleepMode = 0x11;
        constexpr uint8_t setDisplayOff = 0x28;
        constexpr uint8_t setDisplayOn = 0x29;
        constexpr uint8_t setColumnAddress = 0x2a;
        constexpr uint8_t setPageAddress = 0x2b;
        constexpr uint8_t writeMemoryStart = 0x2c;
        constexpr uint8_t setTearOff = 0x34;
        constexpr uint8_t setTearOn = 0x35;
        constexpr uint8_t setAddressMode = 0x36;
        constexpr uint8_t setPixelFormat = 0x3a;
        constexpr uint8_t writeMemoryContinue = 0x3c;
        constexpr uint8_t writeDisplayBrightness = 0x51;
        constexpr uint8_t writeControlDisplay = 0x53;

        constexpr uint8_t pixelFormat16Bits = 0x55;
        constexpr uint8_t pixelFormat24Bits = 0x77;

        constexpr uint8_t tearingEffectVerticalBlanking = 0x00;
        constexpr uint8_t tearingEffectVerticalAndHorizontalBlanking = 0x01;

        constexpr uint8_t addressModePageOrder = 0x80;
        constexpr uint8_t addressModeColumnOrder = 0x40;
        constexpr uint8_t addressModePageColumnExchange = 0x20;
        constexpr uint8_t addressModeLineOrder = 0x10;
        constexpr uint8_t addressModeBgr = 0x08;
        constexpr uint8_t addressModeLatchOrder = 0x04;
        constexpr uint8_t addressModeFlipHorizontal = 0x02;
        constexpr uint8_t addressModeFlipVertical = 0x01;
    }
}

#endif
