#ifndef DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_DISPLAY_HPP
#define DRIVERS_DISPLAY_MIPI_DSI_MIPI_DSI_DISPLAY_HPP

#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/DsiHost.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/MemoryRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // The pixel format is what ends up on the wire: rgb565Swapped is 16 bits per pixel and rgb888 is 24.
    // With a tearingEffect pin, a write waits for its next rising edge, or for the tearing effect timeout
    class MipiDsiDisplay
        : public MipiDsiPanelCore
        , public hal::Display
    {
    public:
        MipiDsiDisplay(hal::DsiHost& host, hal::GpioPin& reset, hal::GpioPin& tearingEffect, const Panel& panel, hal::PixelFormat format, const infra::Function<void(InitializationResult)>& onInitialized);
        MipiDsiDisplay(const MipiDsiDisplay& other) = delete;
        MipiDsiDisplay& operator=(const MipiDsiDisplay& other) = delete;
        ~MipiDsiDisplay();

        hal::DisplaySize Size() const override;
        hal::PixelFormat Format() const override;
        void WriteWithStride(const hal::DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone) override;

    private:
        void BeforeDisplayOn(const infra::Function<void()>& onDone) override;
        void AfterDisplayOff(const infra::Function<void()>& onDone) override;

        void WriteColumnAddress();
        void WritePageAddress();
        void SetWindowBytes(uint16_t start, uint16_t length);

        void WaitForTearingEffect();
        void TearingEffectSignalled();
        void TearingEffectMissed();
        void StopWaitingForTearingEffect();

        void WriteNextChunk();
        void FinishWrite();
        bool IsPacked() const;
        std::size_t RowBytes() const;
        std::size_t SlabCount() const;
        std::size_t SlabBytes() const;
        std::size_t ChunkBytes() const;

    private:
        hal::PixelFormat format;
        hal::InputPin tearingPin;
        const bool tearingConnected;
        infra::TimerSingleShot writeTimer;
        bool waitingForTearingEffect{ false };

        infra::AutoResetFunction<void()> writeCompletion;
        hal::DisplayArea writeArea{};
        infra::ConstByteRange writePixels;
        std::size_t writeStride{ 0 };
        std::size_t slabIndex{ 0 };
        std::size_t slabOffset{ 0 };
        bool firstChunk{ true };
        std::array<uint8_t, 4> windowBytes{};
    };
}

#endif
