#include "boards/mb1166/Mb1166Setup.hpp"

namespace boards
{
    Mb1166Setup::Mb1166Setup(hal::DsiHost& host, hal::DsiVideoStream& stream, hal::GpioPin& reset, hal::PixelFormat format, const infra::Function<void(drivers::MipiDsiPanelCore::InitializationResult)>& onInitialized)
        : panel(host, stream, reset, mb1166Panel, format, onInitialized)
    {}

    drivers::MipiDsiVideoPanel& Mb1166Setup::Panel()
    {
        return panel;
    }
}
