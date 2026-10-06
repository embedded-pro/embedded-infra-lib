#include "drivers/display/ssd2119/Ssd2119BoostxlK350qvgS1.hpp"

namespace drivers
{
    BoostxlK350qvgS1::BoostxlK350qvgS1(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand, hal::GpioPin& reset, const infra::Function<void()>& onInitialized)
        : bus(spiWithChipSelect, dataCommand)
        , display(bus, reset, boostxlK350qvgS1Panel, hal::PixelFormat::rgb565Swapped, onInitialized)
    {}

    hal::Display& BoostxlK350qvgS1::Display()
    {
        return display;
    }
}
