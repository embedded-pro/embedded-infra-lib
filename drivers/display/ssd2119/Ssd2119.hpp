#ifndef DRIVERS_DISPLAY_SSD2119_SSD2119_HPP
#define DRIVERS_DISPLAY_SSD2119_SSD2119_HPP

#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // Drives a Solomon Systech SSD2119 (320 x 240) through a bus that writes a register index followed by
    // data bytes, high byte first. The pixel bytes are handed to the bus unchanged, so the pixel format
    // describes what that bus puts on the wire: rgb565 and rgb565Swapped select the 65k colour mode and
    // rgb888 the 262k colour mode, of which the controller uses the upper six bits of every byte.
    class Ssd2119
        : public hal::Display
    {
    public:
        struct Step
        {
            uint8_t index;
            uint16_t value;
            uint16_t delayAfterInMilliseconds;
        };

        // The panel specific part of the bring-up. mirrorX and mirrorY tell that the first logical pixel is
        // the last GDDRAM position of that axis. The entry mode register is written between the two sequences.
        struct Panel
        {
            hal::DisplaySize size;
            bool mirrorX;
            bool mirrorY;
            infra::MemoryRange<const Step> beforeEntryMode;
            infra::MemoryRange<const Step> afterEntryMode;
        };

        Ssd2119(services::RegisterBusAccess& bus, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, const infra::Function<void()>& onInitialized);

        hal::DisplaySize Size() const override;
        hal::PixelFormat Format() const override;
        void WriteWithStride(const hal::DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone) override;

    private:
        enum class Phase : uint8_t
        {
            resetting,
            beforeEntryMode,
            entryMode,
            afterEntryMode,
            ready
        };

        struct AxisRange
        {
            uint16_t first;
            uint16_t last;
            uint16_t start;
        };

        static constexpr std::size_t windowRegisterCount = 5;

        static AxisRange MapAxis(bool mirrored, uint16_t extent, uint16_t position, uint16_t length);

        void ReleaseReset();
        void NextInitializationStep();
        bool RunStep(infra::MemoryRange<const Step> steps);
        void WriteInitializationRegister(const Step& step);
        void InitializationRegisterWritten();
        uint16_t EntryMode() const;

        void NextWriteStep();
        void NextPixelStep();
        void WriteWindowRegister(std::size_t step);
        void WritePixelRows(std::size_t rows);
        void WriteStridedRow();
        void FinishWrite();
        bool IsPacked() const;
        std::size_t RowBytes() const;
        AxisRange HorizontalRange() const;
        AxisRange VerticalRange() const;

        void WriteRegister(uint8_t index, uint16_t value, const infra::Function<void()>& onDone);

    private:
        services::RegisterBusAccess& bus;
        const Panel& panel;
        hal::PixelFormat format;
        hal::OutputPin resetPin;
        infra::TimerSingleShot timer;
        infra::AutoResetFunction<void()> initialized;
        Phase phase{ Phase::resetting };
        std::size_t stepIndex{ 0 };
        uint16_t stepDelayInMilliseconds{ 0 };

        infra::AutoResetFunction<void()> writeCompletion;
        hal::DisplayArea writeArea{};
        infra::ConstByteRange writePixels;
        std::size_t writeStride{ 0 };
        std::size_t writeStep{ 0 };
        std::size_t writeRow{ 0 };
        std::array<uint8_t, 2> valueBytes{};
    };
}

#endif
