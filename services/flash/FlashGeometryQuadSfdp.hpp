#ifndef SERVICES_FLASH_GEOMETRY_QUAD_SFDP_HPP
#define SERVICES_FLASH_GEOMETRY_QUAD_SFDP_HPP

#include "hal/interfaces/QuadSpi.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Sequencer.hpp"
#include "services/flash/FlashGeometry.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class FlashGeometryQuadSfdp
        : public FlashGeometry
    {
    public:
        static constexpr uint8_t commandReadSfdp = 0x5A;
        static constexpr uint8_t commandWriteEnable = 0x06;
        static constexpr uint8_t commandReadStatusRegister1 = 0x05;
        static constexpr uint8_t commandReadStatusRegister2 = 0x35;
        static constexpr uint8_t commandWriteStatusRegister = 0x01;
        static constexpr uint8_t commandWriteStatusRegister2 = 0x31;
        static constexpr uint8_t commandWriteStatusRegister2Alt = 0x3E;
        static constexpr uint8_t commandReadStatusRegister2Alt = 0x3F;
        static constexpr uint8_t commandEraseBulk = 0xC7;
        static constexpr uint8_t commandPageProgram = 0x32;

        FlashGeometryQuadSfdp(hal::QuadSpi& spi, infra::Function<void()> onInitialized);

        // FlashGeometry interface
        uint32_t NrOfSubSectors() const override;
        uint32_t SizeSector() const override;
        uint32_t SizeSubSector() const override;
        uint32_t SizePage() const override;
        bool ExtendedAddressing() const override;

        // Extended getters for FlashQuadSpiGeneric
        uint8_t EraseSubSectorCommand() const;
        uint8_t EraseSectorCommand() const;
        uint8_t EraseBulkCommand() const;
        uint8_t PageProgramCommand() const;
        uint8_t ReadDataCommand() const;
        uint8_t ReadDummyCycles() const;

    private:
        void ReadSfdpAndParamHeader();
        void ReadBfpt();
        void ParseBfpt();
        void EnableQuadMode();
        void EnableQuad_Qer1_5();
        void EnableQuad_Qer2();
        void EnableQuad_Qer3();
        void EnableQuad_Qer4();

    private:
        hal::QuadSpi& spi;
        infra::Function<void()> onInitialized;
        infra::Sequencer sequencer;

        infra::BoundedVector<uint8_t>::WithMaxSize<4> sfdpAddress;
        std::array<uint8_t, 16> sfdpAndParamHeader;
        std::array<uint8_t, 64> bfptBuffer;
        std::array<uint8_t, 2> statusBuffer;

        uint32_t bfptAddress = 0;
        uint8_t bfptTableLength = 0;
        uint8_t qer = 0;

        uint32_t nrOfSubSectors = 512;
        uint32_t sizeSector = 65536;
        uint32_t sizeSubSector = 4096;
        uint32_t sizePage = 256;
        bool extendedAddressing = false;

        uint8_t eraseSubSectorCommand = 0x20;
        uint8_t eraseSectorCommand = 0xD8;
        uint8_t readDataCommand = 0xEB;
        uint8_t readDummyCycles = 10;
    };
}

#endif
