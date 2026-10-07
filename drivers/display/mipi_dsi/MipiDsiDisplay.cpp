#include "drivers/display/mipi_dsi/MipiDsiDisplay.hpp"
#include "drivers/display/mipi_dsi/MipiDcs.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    namespace
    {
        constexpr std::array<uint8_t, 1> tearingEffectParameter{ dcs::tearingEffectVerticalBlanking };
        constexpr std::array<MipiDsiPanelCore::Command, 1> tearingEffectOn{ { { MipiDsiPanelCore::Packet::dcs, dcs::setTearOn, tearingEffectParameter, 0 } } };

        infra::MemoryRange<const MipiDsiPanelCore::Command> ExtraCommands(hal::GpioPin& tearingEffect)
        {
            if (&tearingEffect == &hal::dummyPin)
                return infra::MemoryRange<const MipiDsiPanelCore::Command>();

            return infra::MakeRange(tearingEffectOn);
        }
    }

    MipiDsiDisplay::MipiDsiDisplay(hal::DsiHost& host, hal::GpioPin& reset, hal::GpioPin& tearingEffect, const Panel& panel, hal::PixelFormat format, const infra::Function<void(InitializationResult)>& onInitialized)
        : MipiDsiPanelCore(host, reset, panel, format, ExtraCommands(tearingEffect))
        , format(format)
        , tearingPin(tearingEffect)
        , tearingConnected(&tearingEffect != &hal::dummyPin)
    {
        really_assert(format != hal::PixelFormat::rgb565);
        really_assert(host.MaxParametersSize() >= windowBytes.size());

        StartInitialization(onInitialized);
    }

    MipiDsiDisplay::~MipiDsiDisplay()
    {
        if (tearingConnected)
            tearingPin.DisableInterrupt();
    }

    hal::DisplaySize MipiDsiDisplay::Size() const
    {
        return Configuration().size;
    }

    hal::PixelFormat MipiDsiDisplay::Format() const
    {
        return format;
    }

    void MipiDsiDisplay::WriteWithStride(const hal::DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone)
    {
        really_assert(Awake());
        really_assert(hal::IsValidDisplayWrite(Configuration().size, format, area, pixels, strideInBytes));
        BeginHostOperation();

        writeCompletion = onDone;
        writeArea = area;
        writePixels = pixels;
        writeStride = strideInBytes;
        slabIndex = 0;
        slabOffset = 0;
        firstChunk = true;

        if (area.width == 0 || area.height == 0)
            writeTimer.Start(infra::Duration::zero(), [this]()
                {
                    FinishWrite();
                });
        else
            WriteColumnAddress();
    }

    void MipiDsiDisplay::BeforeDisplayOn(const infra::Function<void()>& onDone)
    {
        onDone();
    }

    void MipiDsiDisplay::AfterDisplayOff(const infra::Function<void()>& onDone)
    {
        onDone();
    }

    void MipiDsiDisplay::WriteColumnAddress()
    {
        SetWindowBytes(writeArea.x, writeArea.width);
        Host().WriteDcs(dcs::setColumnAddress, infra::MakeRange(windowBytes), [this]()
            {
                WritePageAddress();
            });
    }

    void MipiDsiDisplay::WritePageAddress()
    {
        SetWindowBytes(writeArea.y, writeArea.height);
        Host().WriteDcs(dcs::setPageAddress, infra::MakeRange(windowBytes), [this]()
            {
                WaitForTearingEffect();
            });
    }

    void MipiDsiDisplay::SetWindowBytes(uint16_t start, uint16_t length)
    {
        uint16_t end = static_cast<uint16_t>(start + length - 1);
        windowBytes = { static_cast<uint8_t>(start >> 8), static_cast<uint8_t>(start), static_cast<uint8_t>(end >> 8), static_cast<uint8_t>(end) };
    }

    void MipiDsiDisplay::WaitForTearingEffect()
    {
        if (!tearingConnected)
        {
            WriteNextChunk();
            return;
        }

        waitingForTearingEffect = true;
        tearingPin.EnableInterrupt([this]()
            {
                TearingEffectSignalled();
            },
            hal::InterruptTrigger::risingEdge, hal::InterruptType::dispatched);
        writeTimer.Start(Configuration().timings.tearingEffectTimeout, [this]()
            {
                TearingEffectMissed();
            });
    }

    void MipiDsiDisplay::TearingEffectSignalled()
    {
        if (!waitingForTearingEffect)
            return;

        writeTimer.Cancel();
        StopWaitingForTearingEffect();
    }

    void MipiDsiDisplay::TearingEffectMissed()
    {
        StopWaitingForTearingEffect();
    }

    void MipiDsiDisplay::StopWaitingForTearingEffect()
    {
        waitingForTearingEffect = false;
        tearingPin.DisableInterrupt();
        WriteNextChunk();
    }

    void MipiDsiDisplay::WriteNextChunk()
    {
        if (slabOffset == SlabBytes())
        {
            ++slabIndex;
            slabOffset = 0;
        }

        if (slabIndex == SlabCount())
        {
            FinishWrite();
            return;
        }

        std::size_t size = std::min(ChunkBytes(), SlabBytes() - slabOffset);
        infra::ConstByteRange chunk = infra::Head(infra::DiscardHead(writePixels, slabIndex * writeStride + slabOffset), size);
        uint8_t command = firstChunk ? dcs::writeMemoryStart : dcs::writeMemoryContinue;

        slabOffset += size;
        firstChunk = false;
        Host().WriteDcs(command, chunk, [this]()
            {
                WriteNextChunk();
            });
    }

    void MipiDsiDisplay::FinishWrite()
    {
        EndHostOperation();
        writeCompletion();
    }

    bool MipiDsiDisplay::IsPacked() const
    {
        return writeArea.height == 1 || writeStride == RowBytes();
    }

    std::size_t MipiDsiDisplay::RowBytes() const
    {
        return writeArea.width * hal::BytesPerPixel(format);
    }

    std::size_t MipiDsiDisplay::SlabCount() const
    {
        return IsPacked() ? 1 : writeArea.height;
    }

    std::size_t MipiDsiDisplay::SlabBytes() const
    {
        return IsPacked() ? RowBytes() * writeArea.height : RowBytes();
    }

    std::size_t MipiDsiDisplay::ChunkBytes() const
    {
        std::size_t pixelBytes = hal::BytesPerPixel(format);
        return Host().MaxParametersSize() / pixelBytes * pixelBytes;
    }
}
