#include "services/flash/FlashQuadSpi.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    FlashQuadSpi::FlashQuadSpi(hal::QuadSpi& spi, const FlashGeometry& geometry)
        : hal::FlashHomogeneous(geometry.NrOfSubSectors(), geometry.SizeSubSector())
        , spi(spi)
    {}

    void FlashQuadSpi::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        this->onDone = onDone;
        this->buffer = buffer;
        this->address = address;

        WriteBufferSequence();
    }

    void FlashQuadSpi::EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone)
    {
        this->onDone = onDone;
        sectorIndex = beginIndex;
        sequencer.Load([this, endIndex]()
            {
                sequencer.While([this, endIndex]()
                    {
                        return sectorIndex != endIndex;
                    });
                sequencer.Step([this]()
                    {
                        WriteEnable();
                    });
                sequencer.Step([this, endIndex]()
                    {
                        EraseSomeSectors(endIndex);
                    });
                sequencer.Step([this]()
                    {
                        HoldWhileWriteInProgress();
                    });
                sequencer.EndWhile();
                ScheduleOnDone();
            });
    }

    void FlashQuadSpi::WriteBufferSequence()
    {
        sequencer.Load([this]()
            {
                sequencer.While([this]()
                    {
                        return !this->buffer.empty();
                    });
                sequencer.Step([this]()
                    {
                        WriteEnable();
                    });
                sequencer.Step([this]()
                    {
                        PageProgram();
                    });
                sequencer.Step([this]()
                    {
                        HoldWhileWriteInProgress();
                    });
                sequencer.EndWhile();
                ScheduleOnDone();
            });
    }

    infra::BoundedVector<uint8_t>::WithMaxSize<4> FlashQuadSpi::ConvertAddress(uint32_t address) const
    {
        return hal::QuadSpi::AddressToVector(address, 3);
    }

    void FlashQuadSpi::ScheduleOnDone()
    {
        sequencer.Execute([this]()
            {
                infra::EventDispatcher::Instance().Schedule([this]()
                    {
                        this->onDone();
                    });
            });
    }
}
