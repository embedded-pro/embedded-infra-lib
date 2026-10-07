#ifndef DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_VIDEO_PANEL_HPP
#define DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_VIDEO_PANEL_HPP

#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "hal/interfaces/DsiHost.hpp"
#include "hal/interfaces/Gpio.hpp"

namespace drivers
{
    // The stream starts after sleep out and before display on, and stops after display off.
    // rgb565 and rgb565Swapped both mean 16 bits per pixel
    class MipiDsiVideoPanel
        : public MipiDsiPanelCore
    {
    public:
        MipiDsiVideoPanel(hal::DsiHost& host, hal::DsiVideoStream& stream, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, const infra::Function<void(InitializationResult)>& onInitialized);
        MipiDsiVideoPanel(const MipiDsiVideoPanel& other) = delete;
        MipiDsiVideoPanel& operator=(const MipiDsiVideoPanel& other) = delete;
        ~MipiDsiVideoPanel() = default;

    private:
        void BeforeDisplayOn(const infra::Function<void()>& onDone) override;
        void AfterDisplayOff(const infra::Function<void()>& onDone) override;

    private:
        hal::DsiVideoStream& stream;
    };
}

#endif
