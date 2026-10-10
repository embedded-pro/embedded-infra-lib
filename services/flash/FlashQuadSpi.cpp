#include "services/flash/FlashQuadSpi.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <algorithm>
#include <array>

namespace services
{
    namespace
    {
        struct CommandPair
        {
            uint8_t threeByteAddress;
            uint8_t fourByteAddress;
        };

        constexpr std::array<CommandPair, 12> fourByteAddressCommands{ { { 0x03, 0x13 },
            { 0x0b, 0x0c },
            { 0x3b, 0x3c },
            { 0x6b, 0x6c },
            { 0xbb, 0xbc },
            { 0xeb, 0xec },
            { 0x02, 0x12 },
            { 0x32, 0x34 },
            { 0x38, 0x3e },
            { 0x20, 0x21 },
            { 0x52, 0x5c },
            { 0xd8, 0xdc } } };
    }

    FlashQuadSpi::FlashQuadSpi(hal::QuadSpi& spi, const FlashGeometry& geometry)
        : hal::FlashHomogeneous(geometry.NrOfSubSectors(), geometry.SizeSubSector())
        , spi(spi)
        , flashGeometry(geometry)
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
        return hal::QuadSpi::AddressToVector(address, flashGeometry.ExtendedAddressing() ? 4 : 3);
    }

    uint8_t FlashQuadSpi::AddressedCommand(uint8_t command) const
    {
        if (!flashGeometry.ExtendedAddressing())
            return command;

        const auto pair = std::find_if(fourByteAddressCommands.begin(), fourByteAddressCommands.end(), [command](const CommandPair& candidate)
            {
                return candidate.threeByteAddress == command;
            });

        return pair != fourByteAddressCommands.end() ? pair->fourByteAddress : command;
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
