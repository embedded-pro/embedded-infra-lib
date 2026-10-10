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

    uint8_t FlashGeometrySfdpParser::ReadAddressLinesValue() const
    {
        return readAddressLines;
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
        const uint32_t dword1 = ReadBfptDword(0);
        const uint64_t totalBytes = ParseDensityAndAddressMode(dword1, ReadBfptDword(1));
        ParseFastReadQuad(dword1, ReadBfptDword(2));
        ParseEraseTypes();

        if (totalBytes > 0 && sizeSubSector > 0)
            nrOfSubSectors = static_cast<uint32_t>(totalBytes / sizeSubSector);

        ParsePageSize();
        ParseQer();
    }

    uint64_t FlashGeometrySfdpParser::ParseDensityAndAddressMode(uint32_t dword1, uint32_t dword2)
    {
        // DW1 bits [18:17]: 0 = 3-byte addresses only, 1 = 3- or 4-byte addresses, 2 = 4-byte addresses only
        const uint8_t addrMode = (dword1 >> 17) & 0x03;

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

    void FlashGeometrySfdpParser::ParseFastReadQuad(uint32_t dword1, uint32_t dword3)
    {
        // DW1 bit 21 and 22 tell which quad fast reads exist; DW3 holds the (1-1-4) read in its upper half and the (1-4-4) read in its lower half,
        // each as a number of wait states in bits [4:0], a number of mode clocks in bits [7:5] and an instruction in bits [15:8].
        // There is no mode byte to send, so the mode clocks count as dummy cycles.
        static constexpr uint32_t fastRead144Supported = 1u << 21;
        static constexpr uint32_t fastRead114Supported = 1u << 22;

        const auto parse = [this](uint32_t field, uint8_t addressLines)
        {
            readDataCommand = (field >> 8) & 0xFF;
            readDummyCycles = ((field >> 5) & 0x07) + (field & 0x1F);
            readAddressLines = addressLines;
        };

        if (dword1 & fastRead144Supported)
            parse(dword3 & 0xFFFF, 4);
        else if (dword1 & fastRead114Supported)
            parse(dword3 >> 16, 1);
    }

    void FlashGeometrySfdpParser::ParseEraseTypes()
    {
        // The four erase types are in DW8 and DW9
        if (bfptTableLength < 9)
            return;

        struct EraseType
        {
            uint32_t size;
            uint8_t command;
        };

        auto makeEraseType = [](uint8_t exp, uint8_t cmd) -> EraseType
        {
            return { exp == 0 || exp >= 32 ? 0u : (1u << exp), cmd };
        };

        const uint32_t dword8 = ReadBfptDword(7);
        const uint32_t dword9 = ReadBfptDword(8);
        const std::array<EraseType, 4> types = {
            makeEraseType(dword8 & 0xFF, (dword8 >> 8) & 0xFF),
            makeEraseType((dword8 >> 16) & 0xFF, (dword8 >> 24) & 0xFF),
            makeEraseType(dword9 & 0xFF, (dword9 >> 8) & 0xFF),
            makeEraseType((dword9 >> 16) & 0xFF, (dword9 >> 24) & 0xFF),
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
        if (bfptTableLength < 15)
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
