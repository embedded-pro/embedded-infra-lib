#include "services/flash/FlashGeometrySfdpBase.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    FlashGeometrySfdpParser::FlashGeometrySfdpParser(infra::Function<void()> onInitialized)
        : onInitialized(onInitialized)
    {
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                Transition(ReadingHeader{});
            });
    }

    uint32_t FlashGeometrySfdpParser::NrOfSubSectorsValue() const
    {
        return nrOfSubSectors;
    }

    uint32_t FlashGeometrySfdpParser::SizeSectorValue() const
    {
        return sizeSector;
    }

    uint32_t FlashGeometrySfdpParser::SizeSubSectorValue() const
    {
        return sizeSubSector;
    }

    uint32_t FlashGeometrySfdpParser::SizePageValue() const
    {
        return sizePage;
    }

    bool FlashGeometrySfdpParser::ExtendedAddressingValue() const
    {
        return extendedAddressing;
    }

    uint8_t FlashGeometrySfdpParser::EraseSubSectorCommandValue() const
    {
        return eraseSubSectorCommand;
    }

    uint8_t FlashGeometrySfdpParser::EraseSectorCommandValue() const
    {
        return eraseSectorCommand;
    }

    uint8_t FlashGeometrySfdpParser::ReadDataCommandValue() const
    {
        return readDataCommand;
    }

    uint8_t FlashGeometrySfdpParser::ReadDummyCyclesValue() const
    {
        return readDummyCycles;
    }

    uint8_t FlashGeometrySfdpParser::QerValue() const
    {
        return qer;
    }

    void FlashGeometrySfdpParser::OnBfptParsed(infra::Function<void()> onDone)
    {
        onDone();
    }

    void FlashGeometrySfdpParser::Transition(State newState)
    {
        currentState = newState;
        std::visit([this](auto& s)
            {
                Handle(s);
            },
            currentState);
    }

    void FlashGeometrySfdpParser::Handle(ReadingHeader&)
    {
        PerformRead(0x000000, infra::MakeByteRange(sfdpAndParamHeader), [this]()
            {
                if (ParseSfdpHeader())
                    Transition(ReadingBfpt{});
                else
                    infra::EventDispatcher::Instance().Schedule([this]()
                        {
                            onInitialized();
                        });
            });
    }

    void FlashGeometrySfdpParser::Handle(ReadingBfpt&)
    {
        PerformRead(bfptAddress, infra::MakeByteRange(bfptBuffer), [this]()
            {
                ParseBfpt();
                OnBfptParsed([this]()
                    {
                        infra::EventDispatcher::Instance().Schedule([this]()
                            {
                                onInitialized();
                            });
                    });
            });
    }

    bool FlashGeometrySfdpParser::ParseSfdpHeader()
    {
        static constexpr std::array<uint8_t, 4> signature{ 0x53, 0x46, 0x44, 0x50 };
        if (sfdpAndParamHeader[0] != signature[0] ||
            sfdpAndParamHeader[1] != signature[1] ||
            sfdpAndParamHeader[2] != signature[2] ||
            sfdpAndParamHeader[3] != signature[3])
            return false;

        bfptTableLength = sfdpAndParamHeader[11];
        bfptAddress = sfdpAndParamHeader[12] |
                      (static_cast<uint32_t>(sfdpAndParamHeader[13]) << 8) |
                      (static_cast<uint32_t>(sfdpAndParamHeader[14]) << 16);
        return bfptAddress != 0;
    }

    void FlashGeometrySfdpParser::ParseBfpt()
    {
        const uint64_t totalBytes = ParseDensityAndAddressMode(ReadBfptDword(0), ReadBfptDword(1));
        ParseFastReadQuad(ReadBfptDword(2));
        ParseEraseTypes(ReadBfptDword(3), ReadBfptDword(4), totalBytes);
        ParsePageSize();
        ParseQer();
    }

    uint64_t FlashGeometrySfdpParser::ParseDensityAndAddressMode(uint32_t dword1, uint32_t dword2)
    {
        const uint8_t addrMode = dword1 & 0x07;

        uint64_t totalBytes = 0;
        if (dword2 & 0x80000000u)
        {
            const uint32_t exp = dword2 & 0x7FFFFFFFu;
            if (exp >= 3)
                totalBytes = 1ULL << (exp - 3);
        }
        else
            totalBytes = (static_cast<uint64_t>(dword2) + 1) / 8;

        if (addrMode == 2 || (addrMode == 1 && totalBytes > 0x1000000))
            extendedAddressing = true;

        return totalBytes;
    }

    void FlashGeometrySfdpParser::ParseFastReadQuad(uint32_t dword3)
    {
        if (dword3 & 0x01)
        {
            readDataCommand = (dword3 >> 24) & 0xFF;
            readDummyCycles = (dword3 >> 16) & 0x1F;
        }
    }

    void FlashGeometrySfdpParser::ParseEraseTypes(uint32_t dword4, uint32_t dword5, uint64_t totalBytes)
    {
        struct EraseType
        {
            uint32_t size;
            uint8_t command;
        };

        auto makeEraseType = [](uint8_t exp, uint8_t cmd) -> EraseType
        {
            return { exp == 0 ? 0u : (1u << exp), cmd };
        };

        const std::array<EraseType, 4> types = {
            makeEraseType(dword4 & 0xFF, (dword4 >> 8) & 0xFF),
            makeEraseType((dword4 >> 16) & 0xFF, (dword4 >> 24) & 0xFF),
            makeEraseType(dword5 & 0xFF, (dword5 >> 8) & 0xFF),
            makeEraseType((dword5 >> 16) & 0xFF, (dword5 >> 24) & 0xFF),
        };

        uint32_t smallest = 0;
        uint32_t largest = 0;
        uint8_t smallestCmd = eraseSubSectorCommand;
        uint8_t largestCmd = eraseSectorCommand;
        for (const auto& t : types)
        {
            if (t.size == 0)
                continue;
            if (smallest == 0 || t.size < smallest)
            {
                smallest = t.size;
                smallestCmd = t.command;
            }
            if (t.size > largest)
            {
                largest = t.size;
                largestCmd = t.command;
            }
        }

        if (smallest > 0)
        {
            sizeSubSector = smallest;
            eraseSubSectorCommand = smallestCmd;
        }
        if (largest > sizeSubSector)
        {
            sizeSector = largest;
            eraseSectorCommand = largestCmd;
        }
        else
            sizeSector = sizeSubSector;

        if (totalBytes > 0 && sizeSubSector > 0)
            nrOfSubSectors = static_cast<uint32_t>(totalBytes / sizeSubSector);
    }

    void FlashGeometrySfdpParser::ParsePageSize()
    {
        if (bfptTableLength < 11)
            return;

        const uint32_t dword11 = ReadBfptDword(10);
        const uint8_t pageSizeExp = (dword11 >> 4) & 0x0F;
        if (pageSizeExp > 0)
            sizePage = 1u << pageSizeExp;
    }

    void FlashGeometrySfdpParser::ParseQer()
    {
        if (bfptTableLength < 14)
            return;

        const uint32_t dword15 = ReadBfptDword(14);
        qer = (dword15 >> 20) & 0x07;
    }

    uint32_t FlashGeometrySfdpParser::ReadBfptDword(uint8_t dwordIndex) const
    {
        const uint8_t i = dwordIndex * 4;
        return static_cast<uint32_t>(bfptBuffer[i]) |
               (static_cast<uint32_t>(bfptBuffer[i + 1]) << 8) |
               (static_cast<uint32_t>(bfptBuffer[i + 2]) << 16) |
               (static_cast<uint32_t>(bfptBuffer[i + 3]) << 24);
    }

    FlashGeometrySfdpBase::FlashGeometrySfdpBase(infra::Function<void()> onInitialized)
        : FlashGeometrySfdpParser(onInitialized)
    {}

    uint32_t FlashGeometrySfdpBase::NrOfSubSectors() const
    {
        return NrOfSubSectorsValue();
    }

    uint32_t FlashGeometrySfdpBase::SizeSector() const
    {
        return SizeSectorValue();
    }

    uint32_t FlashGeometrySfdpBase::SizeSubSector() const
    {
        return SizeSubSectorValue();
    }

    uint32_t FlashGeometrySfdpBase::SizePage() const
    {
        return SizePageValue();
    }

    bool FlashGeometrySfdpBase::ExtendedAddressing() const
    {
        return ExtendedAddressingValue();
    }
}
