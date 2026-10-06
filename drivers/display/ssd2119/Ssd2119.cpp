#include "drivers/display/ssd2119/Ssd2119.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <chrono>

namespace drivers
{
    namespace
    {
        constexpr uint8_t entryModeRegister = 0x11;
        constexpr uint8_t ramDataRegister = 0x22;
        constexpr uint8_t verticalRamPositionRegister = 0x44;
        constexpr uint8_t horizontalRamStartRegister = 0x45;
        constexpr uint8_t horizontalRamEndRegister = 0x46;
        constexpr uint8_t ramAddressXRegister = 0x4e;
        constexpr uint8_t ramAddressYRegister = 0x4f;

        constexpr uint16_t colourMode65k = 0x6000;
        constexpr uint16_t colourMode262k = 0x4000;
        constexpr uint16_t denMode = 0x0800;
        constexpr uint16_t horizontalIncrement = 0x0010;
        constexpr uint16_t verticalIncrement = 0x0020;

        constexpr infra::Duration resetPulse = std::chrono::milliseconds(10);
        constexpr infra::Duration resetRecovery = std::chrono::milliseconds(20);
    }

    Ssd2119::Ssd2119(services::RegisterBusAccess& bus, hal::GpioPin& reset, const Panel& panel, hal::PixelFormat format, const infra::Function<void()>& onInitialized)
        : bus(bus)
        , panel(panel)
        , format(format)
        , resetPin(reset, true)
        , initialized(onInitialized)
    {
        really_assert(format != hal::PixelFormat::grey8);

        resetPin.Set(false);
        timer.Start(resetPulse, [this]()
            {
                ReleaseReset();
            });
    }

    hal::DisplaySize Ssd2119::Size() const
    {
        return panel.size;
    }

    hal::PixelFormat Ssd2119::Format() const
    {
        return format;
    }

    void Ssd2119::WriteWithStride(const hal::DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone)
    {
        really_assert(phase == Phase::ready);
        really_assert(!writeCompletion);
        really_assert(hal::IsValidDisplayWrite(panel.size, format, area, pixels, strideInBytes));

        writeCompletion = onDone;
        writeArea = area;
        writePixels = pixels;
        writeStride = strideInBytes;
        writeStep = 0;
        writeRow = 0;

        if (area.width == 0 || area.height == 0)
            timer.Start(infra::Duration::zero(), [this]()
                {
                    FinishWrite();
                });
        else
            NextWriteStep();
    }

    Ssd2119::AxisRange Ssd2119::MapAxis(bool mirrored, uint16_t extent, uint16_t position, uint16_t length)
    {
        if (mirrored)
            return { static_cast<uint16_t>(extent - position - length), static_cast<uint16_t>(extent - 1 - position), static_cast<uint16_t>(extent - 1 - position) };

        return { position, static_cast<uint16_t>(position + length - 1), position };
    }

    void Ssd2119::ReleaseReset()
    {
        resetPin.Set(true);
        timer.Start(resetRecovery, [this]()
            {
                phase = Phase::beforeEntryMode;
                stepIndex = 0;
                NextInitializationStep();
            });
    }

    void Ssd2119::NextInitializationStep()
    {
        switch (phase)
        {
            case Phase::beforeEntryMode:
                if (!RunStep(panel.beforeEntryMode))
                {
                    phase = Phase::entryMode;
                    WriteInitializationRegister({ entryModeRegister, EntryMode(), 0 });
                }
                break;
            case Phase::entryMode:
                phase = Phase::afterEntryMode;
                stepIndex = 0;
                NextInitializationStep();
                break;
            case Phase::afterEntryMode:
                if (!RunStep(panel.afterEntryMode))
                {
                    phase = Phase::ready;
                    initialized();
                }
                break;
            case Phase::resetting:
            case Phase::ready:
                break;
        }
    }

    bool Ssd2119::RunStep(infra::MemoryRange<const Step> steps)
    {
        if (stepIndex == steps.size())
            return false;

        WriteInitializationRegister(steps[stepIndex++]);
        return true;
    }

    void Ssd2119::WriteInitializationRegister(const Step& step)
    {
        stepDelayInMilliseconds = step.delayAfterInMilliseconds;
        WriteRegister(step.index, step.value, [this]()
            {
                InitializationRegisterWritten();
            });
    }

    void Ssd2119::InitializationRegisterWritten()
    {
        if (stepDelayInMilliseconds == 0)
            NextInitializationStep();
        else
            timer.Start(std::chrono::milliseconds(stepDelayInMilliseconds), [this]()
                {
                    NextInitializationStep();
                });
    }

    uint16_t Ssd2119::EntryMode() const
    {
        uint16_t entryMode = format == hal::PixelFormat::rgb888 ? colourMode262k : colourMode65k;
        entryMode |= denMode;

        if (!panel.mirrorY)
            entryMode |= verticalIncrement;

        if (!panel.mirrorX)
            entryMode |= horizontalIncrement;

        return entryMode;
    }

    void Ssd2119::NextWriteStep()
    {
        if (writeStep < windowRegisterCount)
            WriteWindowRegister(writeStep++);
        else
            NextPixelStep();
    }

    void Ssd2119::NextPixelStep()
    {
        if (writeRow == writeArea.height)
            FinishWrite();
        else if (IsPacked())
            WritePixelRows(writeArea.height);
        else
            WriteStridedRow();
    }

    void Ssd2119::WriteWindowRegister(std::size_t step)
    {
        AxisRange horizontal = HorizontalRange();
        AxisRange vertical = VerticalRange();
        uint8_t index = 0;
        uint16_t value = 0;

        switch (step)
        {
            case 0:
                index = horizontalRamStartRegister;
                value = horizontal.first;
                break;
            case 1:
                index = horizontalRamEndRegister;
                value = horizontal.last;
                break;
            case 2:
                index = verticalRamPositionRegister;
                value = static_cast<uint16_t>(vertical.last << 8 | vertical.first);
                break;
            case 3:
                index = ramAddressXRegister;
                value = horizontal.start;
                break;
            default:
                index = ramAddressYRegister;
                value = vertical.start;
                break;
        }

        WriteRegister(index, value, [this]()
            {
                NextWriteStep();
            });
    }

    void Ssd2119::WritePixelRows(std::size_t rows)
    {
        infra::ConstByteRange pixels = infra::Head(infra::DiscardHead(writePixels, writeRow * writeStride), rows * RowBytes());
        writeRow += rows;

        bus.WriteRegister(ramDataRegister, pixels, [this]()
            {
                NextPixelStep();
            });
    }

    void Ssd2119::WriteStridedRow()
    {
        WriteRegister(ramAddressXRegister, HorizontalRange().start, [this]()
            {
                WriteRegister(ramAddressYRegister, MapAxis(panel.mirrorY, panel.size.height, static_cast<uint16_t>(writeArea.y + writeRow), 1).start, [this]()
                    {
                        WritePixelRows(1);
                    });
            });
    }

    void Ssd2119::FinishWrite()
    {
        writeCompletion();
    }

    bool Ssd2119::IsPacked() const
    {
        return writeArea.height == 1 || writeStride == RowBytes();
    }

    std::size_t Ssd2119::RowBytes() const
    {
        return writeArea.width * hal::BytesPerPixel(format);
    }

    Ssd2119::AxisRange Ssd2119::HorizontalRange() const
    {
        return MapAxis(panel.mirrorX, panel.size.width, writeArea.x, writeArea.width);
    }

    Ssd2119::AxisRange Ssd2119::VerticalRange() const
    {
        return MapAxis(panel.mirrorY, panel.size.height, writeArea.y, writeArea.height);
    }

    void Ssd2119::WriteRegister(uint8_t index, uint16_t value, const infra::Function<void()>& onDone)
    {
        valueBytes = { static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) };
        bus.WriteRegister(index, infra::MakeByteRange(valueBytes), onDone);
    }
}
