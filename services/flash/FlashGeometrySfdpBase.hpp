#ifndef SERVICES_FLASH_GEOMETRY_SFDP_BASE_HPP
#define SERVICES_FLASH_GEOMETRY_SFDP_BASE_HPP

#include "infra/util/Function.hpp"
#include "services/flash/FlashGeometry.hpp"
#include <array>
#include <cstdint>
#include <variant>

namespace services
{
    class FlashGeometrySfdpParser
    {
    protected:
        static constexpr uint8_t commandReadSfdp = 0x5A;

        explicit FlashGeometrySfdpParser(infra::Function<void()> onInitialized);

        virtual void PerformRead(uint32_t address, infra::ByteRange buffer, infra::Function<void()> onDone) = 0;
        virtual void OnBfptParsed(infra::Function<void()> onDone);

        uint32_t NrOfSubSectorsValue() const;
        uint32_t SizeSectorValue() const;
        uint32_t SizeSubSectorValue() const;
        uint32_t SizePageValue() const;
        bool ExtendedAddressingValue() const;
        uint8_t EraseSubSectorCommandValue() const;
        uint8_t EraseSectorCommandValue() const;
        uint8_t ReadDataCommandValue() const;
        uint8_t ReadDummyCyclesValue() const;
        uint8_t QerValue() const;

    private:
        struct ReadingHeader
        {};

        struct ReadingBfpt
        {};

        using State = std::variant<ReadingHeader, ReadingBfpt>;

        void Transition(State newState);
        void Handle(ReadingHeader& state);
        void Handle(ReadingBfpt& state);
        bool ParseSfdpHeader();
        void ParseBfpt();
        uint64_t ParseDensityAndAddressMode(uint32_t dword1, uint32_t dword2);
        void ParseFastReadQuad(uint32_t dword3);
        void ParseEraseTypes(uint32_t dword4, uint32_t dword5, uint64_t totalBytes);
        void ParsePageSize();
        void ParseQer();
        uint32_t ReadBfptDword(uint8_t dwordIndex) const;

        infra::Function<void()> onInitialized;
        State currentState{ ReadingHeader{} };

        std::array<uint8_t, 16> sfdpAndParamHeader{};
        std::array<uint8_t, 64> bfptBuffer{};
        uint32_t bfptAddress = 0;
        uint8_t bfptTableLength = 0;

        uint32_t nrOfSubSectors = 512;
        uint32_t sizeSector = 65536;
        uint32_t sizeSubSector = 4096;
        uint32_t sizePage = 256;
        bool extendedAddressing = false;

        uint8_t eraseSubSectorCommand = 0x20;
        uint8_t eraseSectorCommand = 0xD8;
        uint8_t readDataCommand = 0xEB;
        uint8_t readDummyCycles = 10;
        uint8_t qer = 0;
    };

    class FlashGeometrySfdpBase
        : public FlashGeometry
        , private FlashGeometrySfdpParser
    {
    public:
        explicit FlashGeometrySfdpBase(infra::Function<void()> onInitialized);

        uint32_t NrOfSubSectors() const final;
        uint32_t SizeSector() const final;
        uint32_t SizeSubSector() const final;
        uint32_t SizePage() const final;
        bool ExtendedAddressing() const final;

    protected:
        using FlashGeometrySfdpParser::commandReadSfdp;
        using FlashGeometrySfdpParser::EraseSectorCommandValue;
        using FlashGeometrySfdpParser::EraseSubSectorCommandValue;
        using FlashGeometrySfdpParser::OnBfptParsed;
        using FlashGeometrySfdpParser::PerformRead;
        using FlashGeometrySfdpParser::QerValue;
        using FlashGeometrySfdpParser::ReadDataCommandValue;
        using FlashGeometrySfdpParser::ReadDummyCyclesValue;
    };
}

#endif
