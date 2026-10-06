#include "boards/boostxl_k350qvg_s1/BoostxlK350qvgS1Setup.hpp"

namespace boards
{
    BoostxlK350qvgS1Setup::BoostxlK350qvgS1Setup(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand, hal::GpioPin& reset, const infra::Function<void()>& onInitialized)
        : bus(spiWithChipSelect, dataCommand)
        , display(bus, reset, boostxlK350qvgS1Panel, hal::PixelFormat::rgb565Swapped, onInitialized)
    {}

    hal::Display& BoostxlK350qvgS1Setup::Display()
    {
        return display;
    }
}
