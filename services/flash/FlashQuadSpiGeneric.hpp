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

        // The lines that carry the instruction, the address and the data of the commands, which depend on the mode the flash is in
        struct Protocol
        {
            hal::QuadSpi::Lines command;
            hal::QuadSpi::Lines read;
            hal::QuadSpi::Lines program;

            // The flash is in quad I/O (QPI) mode: every phase of every command uses four lines
            static Protocol Quad();
            // The flash is in its power-up mode: commands use one line, a read sends its address and data on four and a program sends its data on four
            static Protocol ExtendedSpi();
        };

        FlashQuadSpiGeneric(hal::QuadSpi& spi, const FlashGeometryQuad& geometry, const Protocol& protocol = Protocol::Quad());

        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;

    private:
        void PageProgram() override;
        void WriteEnable() override;
        void EraseSomeSectors(uint32_t endIndex) override;
        void SendEraseSubSector(uint32_t sectorIndex);
        void SendEraseSector(uint32_t sectorIndex);
        void SendEraseBulk();
        void HoldWhileWriteInProgress() override;

    private:
        const FlashGeometryQuad& geometry;
        Protocol protocol;
    };
}

#endif
