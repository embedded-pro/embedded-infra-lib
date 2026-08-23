#include "services/flash/FlashGeometrySfdp.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    FlashGeometrySfdp::FlashGeometrySfdp(hal::SpiMaster& spi, infra::Function<void()> onInitialized)
        : spi(spi)
        , onInitialized(onInitialized)
    {
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                sequencer.Load([this]()
                    {
                        sequencer.Step([this]()
                            {
                                ReadSfdpAndParamHeader();
                            });
                        sequencer.Step([this]()
                            {
                                ReadBfpt();
                            });
                        sequencer.Execute([this]()
                            {
                                infra::EventDispatcher::Instance().Schedule([this]()
                                    {
                                        this->onInitialized();
                                    });
                            });
                    });
            });
    }

    uint32_t FlashGeometrySfdp::NrOfSubSectors() const
    {
        return nrOfSubSectors;
    }

    uint32_t FlashGeometrySfdp::SizeSector() const
    {
        return sizeSector;
    }

    uint32_t FlashGeometrySfdp::SizeSubSector() const
    {
        return sizeSubSector;
    }

    uint32_t FlashGeometrySfdp::SizePage() const
    {
        return sizePage;
    }

    bool FlashGeometrySfdp::ExtendedAddressing() const
    {
        return extendedAddressing;
    }

    void FlashGeometrySfdp::SetCommandAddress(uint32_t address)
    {
        commandBuffer[0] = commandReadSfdp;
        commandBuffer[1] = static_cast<uint8_t>(address >> 16);
        commandBuffer[2] = static_cast<uint8_t>(address >> 8);
        commandBuffer[3] = static_cast<uint8_t>(address);
        commandBuffer[4] = 0xFF; // dummy byte
    }

    void FlashGeometrySfdp::ReadSfdpAndParamHeader()
    {
        SetCommandAddress(0x000000);
        spi.SendData(infra::MakeByteRange(commandBuffer), hal::SpiAction::continueSession, [this]()
            {
                spi.ReceiveData(infra::MakeByteRange(sfdpAndParamHeader), hal::SpiAction::stop, [this]()
                    {
                        static constexpr std::array<uint8_t, 4> sfdpSignature = { 0x53, 0x46, 0x44, 0x50 };
                        const bool validSignature = sfdpAndParamHeader[0] == sfdpSignature[0] &&
                                                    sfdpAndParamHeader[1] == sfdpSignature[1] &&
                                                    sfdpAndParamHeader[2] == sfdpSignature[2] &&
                                                    sfdpAndParamHeader[3] == sfdpSignature[3];

                        if (validSignature)
                        {
                            bfptTableLength = sfdpAndParamHeader[11];
                            bfptAddress = sfdpAndParamHeader[12] |
                                          (static_cast<uint32_t>(sfdpAndParamHeader[13]) << 8) |
                                          (static_cast<uint32_t>(sfdpAndParamHeader[14]) << 16);
                        }

                        sequencer.Continue();
                    });
            });
    }

    void FlashGeometrySfdp::ReadBfpt()
    {
        if (bfptAddress == 0)
        {
            sequencer.Continue();
            return;
        }

        SetCommandAddress(bfptAddress);
        spi.SendData(infra::MakeByteRange(commandBuffer), hal::SpiAction::continueSession, [this]()
            {
                spi.ReceiveData(infra::MakeByteRange(bfptBuffer), hal::SpiAction::stop, [this]()
                    {
                        ParseBfpt();
                        sequencer.Continue();
                    });
            });
    }

    void FlashGeometrySfdp::ParseBfpt()
    {
        auto readDword = [this](uint8_t dwordIndex) -> uint32_t
        {
            const uint8_t byteIndex = dwordIndex * 4;
            return static_cast<uint32_t>(bfptBuffer[byteIndex]) |
                   (static_cast<uint32_t>(bfptBuffer[byteIndex + 1]) << 8) |
                   (static_cast<uint32_t>(bfptBuffer[byteIndex + 2]) << 16) |
                   (static_cast<uint32_t>(bfptBuffer[byteIndex + 3]) << 24);
        };

        // DWORD 1: address bytes
        const uint32_t dword1 = readDword(0);
        const uint8_t addrMode = dword1 & 0x07;
        if (addrMode == 2)
            extendedAddressing = true;

        // DWORD 2: memory density
        const uint32_t dword2 = readDword(1);
        uint64_t totalBytes = 0;
        if (dword2 & 0x80000000u)
        {
            const uint32_t exp = dword2 & 0x7FFFFFFFu;
            if (exp >= 3)
                totalBytes = 1ULL << (exp - 3);
        }
        else
            totalBytes = (static_cast<uint64_t>(dword2) + 1) / 8;

        if (addrMode == 1 && totalBytes > 0x1000000)
            extendedAddressing = true;

        // DWORDs 4 and 5: erase types (1-based DWORD index, 0-based array index)
        const uint32_t dword4 = readDword(3);
        const uint32_t dword5 = readDword(4);

        struct EraseType
        {
            uint32_t size;
            uint8_t command;
        };

        auto makeEraseType = [](uint8_t sizeExp, uint8_t cmd) -> EraseType
        {
            return { sizeExp == 0 ? 0u : (1u << sizeExp), cmd };
        };

        const EraseType eraseTypes[4] = {
            makeEraseType(dword4 & 0xFF, (dword4 >> 8) & 0xFF),
            makeEraseType((dword4 >> 16) & 0xFF, (dword4 >> 24) & 0xFF),
            makeEraseType(dword5 & 0xFF, (dword5 >> 8) & 0xFF),
            makeEraseType((dword5 >> 16) & 0xFF, (dword5 >> 24) & 0xFF),
        };

        uint32_t smallestSize = 0;
        uint32_t largestSize = 0;

        for (const auto& et : eraseTypes)
        {
            if (et.size == 0)
                continue;
            if (smallestSize == 0 || et.size < smallestSize)
                smallestSize = et.size;
            if (et.size > largestSize)
                largestSize = et.size;
        }

        if (smallestSize > 0)
            sizeSubSector = smallestSize;
        if (largestSize > sizeSubSector)
            sizeSector = largestSize;
        else
            sizeSector = sizeSubSector;

        if (totalBytes > 0 && sizeSubSector > 0)
            nrOfSubSectors = static_cast<uint32_t>(totalBytes / sizeSubSector);

        // DWORD 11 (0-based index 10): page size — requires JESD216A or later (tableLength >= 11)
        if (bfptTableLength >= 11)
        {
            const uint32_t dword11 = readDword(10);
            const uint8_t pageSizeExp = (dword11 >> 4) & 0x0F;
            if (pageSizeExp > 0)
                sizePage = 1u << pageSizeExp;
        }
    }
}
