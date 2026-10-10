#ifndef SERVICES_FLASH_QUAD_SPI_GENERIC_HPP
#define SERVICES_FLASH_QUAD_SPI_GENERIC_HPP

#include "services/flash/FlashGeometryQuad.hpp"
#include "services/flash/FlashQuadSpi.hpp"

namespace services
{
    class FlashQuadSpiGeneric
        : public FlashQuadSpi
    {
    public:
        static const uint8_t statusFlagWriteInProgress = 1;

        // The mode the flash is in, which decides on how many lines the instruction, the address and the data of a command are sent
        enum class Protocol : uint8_t
        {
            // Quad I/O (QPI) mode: every phase of every command uses four lines
            quad,
            // Power-up mode: commands use one line, a read sends its address and its data on four lines (1-4-4) or only its data (1-1-4), and a program sends its data on four
            extendedSpi
        };

        FlashQuadSpiGeneric(hal::QuadSpi& spi, const FlashGeometryQuad& geometry, Protocol protocol = Protocol::quad);

        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;

    private:
        void PageProgram() override;
        void WriteEnable() override;
        void EraseSomeSectors(uint32_t endIndex) override;
        void SendEraseSubSector(uint32_t sectorIndex);
        void SendEraseSector(uint32_t sectorIndex);
        void SendEraseBulk();
        void HoldWhileWriteInProgress() override;
        hal::QuadSpi::Lines CommandLines() const;
        hal::QuadSpi::Lines ReadLines() const;
        hal::QuadSpi::Lines ProgramLines() const;

    private:
        const FlashGeometryQuad& geometry;
        Protocol protocol;
    };
}

#endif
