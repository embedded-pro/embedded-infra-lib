#include "drivers/display/mipi_dsi/MipiDsiVideoPanel.hpp"

namespace drivers
{
    MipiDsiVideoPanel::MipiDsiVideoPanel(hal::DsiHost& host, hal::DsiVideoStream& stream, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, const infra::Function<void(InitializationResult)>& onInitialized)
        : MipiDsiPanelCore(host, reset, panel, format, infra::MemoryRange<const Command>())
        , stream(stream)
    {
        StartInitialization(onInitialized);
    }

    void MipiDsiVideoPanel::BeforeDisplayOn(const infra::Function<void()>& onDone)
    {
        stream.Start(onDone);
    }

    void MipiDsiVideoPanel::AfterDisplayOff(const infra::Function<void()>& onDone)
    {
        stream.Stop(onDone);
    }
}
