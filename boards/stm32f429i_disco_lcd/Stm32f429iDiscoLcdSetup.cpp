#include "boards/stm32f429i_disco_lcd/Stm32f429iDiscoLcdSetup.hpp"

namespace boards
{
    Stm32f429iDiscoLcdSetup::Stm32f429iDiscoLcdSetup(hal::SpiMaster& spi, hal::GpioPin& chipSelect, hal::GpioPin& dataCommand, const infra::Function<void()>& onInitialized)
        : spiWithChipSelect(spi, chipSelect)
        , bus(spiWithChipSelect, dataCommand)
        , controller(bus, stm32f429iDiscoLcdPanel, onInitialized)
    {}
}
