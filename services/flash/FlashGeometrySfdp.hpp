#ifndef SERVICES_FLASH_GEOMETRY_SFDP_HPP
#define SERVICES_FLASH_GEOMETRY_SFDP_HPP

#include "hal/interfaces/Spi.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Sequencer.hpp"
#include "services/flash/FlashGeometry.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class FlashGeometrySfdp
        : public FlashGeometry
    {
    public:
        static constexpr uint8_t commandReadSfdp = 0x5A;

        FlashGeometrySfdp(hal::SpiMaster& spi, infra::Function<void()> onInitialized);

        uint32_t NrOfSubSectors() const override;
        uint32_t SizeSector() const override;
        uint32_t SizeSubSector() const override;
        uint32_t SizePage() const override;
        bool ExtendedAddressing() const override;

    private:
        void ReadSfdpAndParamHeader();
        void ReadBfpt();
        void ParseBfpt();
        void SetCommandAddress(uint32_t address);

    private:
        hal::SpiMaster& spi;
        infra::Function<void()> onInitialized;
        infra::Sequencer sequencer;

        std::array<uint8_t, 5> commandBuffer;
        std::array<uint8_t, 16> sfdpAndParamHeader;
        std::array<uint8_t, 64> bfptBuffer;

        uint32_t bfptAddress = 0;
        uint8_t bfptTableLength = 0;

        uint32_t nrOfSubSectors = 512;
        uint32_t sizeSector = 65536;
        uint32_t sizeSubSector = 4096;
        uint32_t sizePage = 256;
        bool extendedAddressing = false;
    };
}

#endif
