#include "services/flash/FlashQuadSpiGeneric.hpp"

namespace services
{
    namespace
    {
        constexpr uint8_t commandWriteEnable = 0x06;
        constexpr uint8_t commandReadStatusRegister = 0x05;
    }

    FlashQuadSpiGeneric::FlashQuadSpiGeneric(hal::QuadSpi& spi, const FlashGeometryQuad& geometry, Protocol protocol)
        : FlashQuadSpi(spi, geometry)
        , geometry(geometry)
        , protocol(protocol)
    {}

    void FlashQuadSpiGeneric::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        const hal::QuadSpi::Header header{ std::make_optional(AddressedCommand(geometry.ReadDataCommand())), ConvertAddress(address), {}, geometry.ReadDummyCycles() };
        spi.ReceiveData(header, buffer, ReadLines(), onDone);
    }

    void FlashQuadSpiGeneric::PageProgram()
    {
        const hal::QuadSpi::Header pageProgramHeader{ std::make_optional(AddressedCommand(geometry.PageProgramCommand())), ConvertAddress(address), {}, 0 };

        infra::ConstByteRange currentBuffer = infra::Head(buffer, geometry.SizePage() - AddressOffsetInSector(address) % geometry.SizePage());
        buffer.pop_front(currentBuffer.size());
        address += currentBuffer.size();

        spi.SendData(pageProgramHeader, currentBuffer, ProgramLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    void FlashQuadSpiGeneric::WriteEnable()
    {
        static const hal::QuadSpi::Header writeEnableHeader{ std::make_optional(commandWriteEnable), {}, {}, 0 };
        spi.SendData(writeEnableHeader, {}, CommandLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    void FlashQuadSpiGeneric::EraseSomeSectors(uint32_t endIndex)
    {
        const uint32_t subSectorsPerSector = geometry.SizeSector() / geometry.SizeSubSector();

        if (sectorIndex == 0 && endIndex == NumberOfSectors())
        {
            SendEraseBulk();
            sectorIndex += NumberOfSectors();
        }
        else if (sectorIndex % subSectorsPerSector == 0 && sectorIndex + subSectorsPerSector <= endIndex)
        {
            SendEraseSector(sectorIndex);
            sectorIndex += subSectorsPerSector;
        }
        else
        {
            SendEraseSubSector(sectorIndex);
            ++sectorIndex;
        }
    }

    void FlashQuadSpiGeneric::SendEraseSubSector(uint32_t sectorIndex)
    {
        const hal::QuadSpi::Header eraseHeader{ std::make_optional(AddressedCommand(geometry.EraseSubSectorCommand())), ConvertAddress(AddressOfSector(sectorIndex)), {}, 0 };
        spi.SendData(eraseHeader, {}, CommandLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    void FlashQuadSpiGeneric::SendEraseSector(uint32_t sectorIndex)
    {
        const hal::QuadSpi::Header eraseHeader{ std::make_optional(AddressedCommand(geometry.EraseSectorCommand())), ConvertAddress(AddressOfSector(sectorIndex)), {}, 0 };
        spi.SendData(eraseHeader, {}, CommandLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    void FlashQuadSpiGeneric::SendEraseBulk()
    {
        static const hal::QuadSpi::Header eraseBulkHeader{ std::make_optional(geometry.EraseBulkCommand()), {}, {}, 0 };
        spi.SendData(eraseBulkHeader, {}, CommandLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    void FlashQuadSpiGeneric::HoldWhileWriteInProgress()
    {
        static const hal::QuadSpi::Header pollHeader{ std::make_optional(commandReadStatusRegister), {}, {}, 0 };
        spi.PollStatus(pollHeader, 1, 0, statusFlagWriteInProgress, CommandLines(), [this]()
            {
                sequencer.Continue();
            });
    }

    hal::QuadSpi::Lines FlashQuadSpiGeneric::CommandLines() const
    {
        return protocol == Protocol::quad ? hal::QuadSpi::Lines::QuadSpeed() : hal::QuadSpi::Lines::SingleSpeed();
    }

    hal::QuadSpi::Lines FlashQuadSpiGeneric::ReadLines() const
    {
        return protocol == Protocol::quad ? hal::QuadSpi::Lines::QuadSpeed() : hal::QuadSpi::Lines::MixedSpeed(1, geometry.ReadAddressLines(), 4);
    }

    hal::QuadSpi::Lines FlashQuadSpiGeneric::ProgramLines() const
    {
        return protocol == Protocol::quad ? hal::QuadSpi::Lines::QuadSpeed() : hal::QuadSpi::Lines::MixedSpeed(1, 1, 4);
    }
}
