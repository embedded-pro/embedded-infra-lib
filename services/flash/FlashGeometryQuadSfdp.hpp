#ifndef SERVICES_FLASH_GEOMETRY_QUAD_SFDP_HPP
#define SERVICES_FLASH_GEOMETRY_QUAD_SFDP_HPP

#include "hal/interfaces/QuadSpi.hpp"
#include "infra/util/Function.hpp"
#include "services/flash/FlashGeometryQuad.hpp"
#include "services/flash/FlashGeometrySfdpBase.hpp"
#include <array>
#include <variant>

namespace services
{
    class FlashGeometryQuadSfdp
        : public FlashGeometryQuad
        , private FlashGeometrySfdpParser
    {
    public:
        static constexpr uint8_t commandEraseBulk = 0xC7;
        static constexpr uint8_t commandPageProgram = 0x32;

        FlashGeometryQuadSfdp(hal::QuadSpi& spi, infra::Function<void()> onInitialized);

        uint32_t NrOfSubSectors() const override;
        uint32_t SizeSector() const override;
        uint32_t SizeSubSector() const override;
        uint32_t SizePage() const override;
        bool ExtendedAddressing() const override;

        uint8_t EraseSubSectorCommand() const override;
        uint8_t EraseSectorCommand() const override;
        uint8_t EraseBulkCommand() const override;
        uint8_t PageProgramCommand() const override;
        uint8_t ReadDataCommand() const override;
        uint8_t ReadDummyCycles() const override;

    private:
        struct QerReadSr2
        {};

        struct QerReadSr1
        {};

        struct QerWriteEnableFor12
        {};

        struct QerWriteSr12
        {};

        struct QerReadSr1Only
        {};

        struct QerWriteEnableFor1
        {};

        struct QerWriteSr1
        {};

        struct QerWriteEnableForAlt
        {};

        struct QerWriteSr2Alt
        {};

        struct QerWriteSr2
        {};

        using QerState = std::variant<
            QerReadSr2, QerReadSr1, QerWriteEnableFor12, QerWriteSr12,
            QerReadSr1Only, QerWriteEnableFor1, QerWriteSr1,
            QerWriteEnableForAlt, QerWriteSr2Alt,
            QerWriteSr2>;

        void PerformRead(uint32_t address, infra::ByteRange buffer, infra::Function<void()> onDone) override;
        void OnBfptParsed(infra::Function<void()> onDone) override;

        void QerTransition(QerState newState);
        void Handle(QerReadSr2& state);
        void Handle(QerReadSr1& state);
        void Handle(QerWriteEnableFor12& state);
        void Handle(QerWriteSr12& state);
        void Handle(QerReadSr1Only& state);
        void Handle(QerWriteEnableFor1& state);
        void Handle(QerWriteSr1& state);
        void Handle(QerWriteEnableForAlt& state);
        void Handle(QerWriteSr2Alt& state);
        void Handle(QerWriteSr2& state);

        hal::QuadSpi& spi;
        infra::BoundedVector<uint8_t>::WithMaxSize<4> sfdpAddressVector;
        std::array<uint8_t, 2> statusBuffer{};
        infra::Function<void()> onQuadDone;
        QerState qerState{ QerReadSr2{} };
    };
}

#endif
