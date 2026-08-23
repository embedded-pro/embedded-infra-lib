#include "services/flash/FlashGeometryQuadSfdp.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    FlashGeometryQuadSfdp::FlashGeometryQuadSfdp(hal::QuadSpi& spi, infra::Function<void()> onInitialized)
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
                        sequencer.Step([this]()
                            {
                                EnableQuadMode();
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

    uint32_t FlashGeometryQuadSfdp::NrOfSubSectors() const
    {
        return nrOfSubSectors;
    }

    uint32_t FlashGeometryQuadSfdp::SizeSector() const
    {
        return sizeSector;
    }

    uint32_t FlashGeometryQuadSfdp::SizeSubSector() const
    {
        return sizeSubSector;
    }

    uint32_t FlashGeometryQuadSfdp::SizePage() const
    {
        return sizePage;
    }

    bool FlashGeometryQuadSfdp::ExtendedAddressing() const
    {
        return extendedAddressing;
    }

    uint8_t FlashGeometryQuadSfdp::EraseSubSectorCommand() const
    {
        return eraseSubSectorCommand;
    }

    uint8_t FlashGeometryQuadSfdp::EraseSectorCommand() const
    {
        return eraseSectorCommand;
    }

    uint8_t FlashGeometryQuadSfdp::EraseBulkCommand() const
    {
        return commandEraseBulk;
    }

    uint8_t FlashGeometryQuadSfdp::PageProgramCommand() const
    {
        return commandPageProgram;
    }

    uint8_t FlashGeometryQuadSfdp::ReadDataCommand() const
    {
        return readDataCommand;
    }

    uint8_t FlashGeometryQuadSfdp::ReadDummyCycles() const
    {
        return readDummyCycles;
    }

    void FlashGeometryQuadSfdp::ReadSfdpAndParamHeader()
    {
        sfdpAddress = hal::QuadSpi::AddressToVector(0x000000, 3);
        const hal::QuadSpi::Header header{ std::make_optional(commandReadSfdp), sfdpAddress, {}, 8 };
        spi.ReceiveData(header, infra::MakeByteRange(sfdpAndParamHeader), hal::QuadSpi::Lines::SingleSpeed(), [this]()
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
    }

    void FlashGeometryQuadSfdp::ReadBfpt()
    {
        if (bfptAddress == 0)
        {
            sequencer.Continue();
            return;
        }

        sfdpAddress = hal::QuadSpi::AddressToVector(bfptAddress, 3);
        const hal::QuadSpi::Header header{ std::make_optional(commandReadSfdp), sfdpAddress, {}, 8 };
        spi.ReceiveData(header, infra::MakeByteRange(bfptBuffer), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                ParseBfpt();
                sequencer.Continue();
            });
    }

    void FlashGeometryQuadSfdp::ParseBfpt()
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

        // DWORD 3: 1-4-4 Fast Read (bit[0] = supported, bits[20:16] = wait states, bits[31:24] = instruction)
        const uint32_t dword3 = readDword(2);
        if (dword3 & 0x01)
        {
            readDataCommand = (dword3 >> 24) & 0xFF;
            readDummyCycles = (dword3 >> 16) & 0x1F;
        }

        // DWORDs 4 and 5: erase types
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
        uint8_t smallestCmd = eraseSubSectorCommand;
        uint32_t largestSize = 0;
        uint8_t largestCmd = eraseSectorCommand;

        for (const auto& et : eraseTypes)
        {
            if (et.size == 0)
                continue;
            if (smallestSize == 0 || et.size < smallestSize)
            {
                smallestSize = et.size;
                smallestCmd = et.command;
            }
            if (et.size > largestSize)
            {
                largestSize = et.size;
                largestCmd = et.command;
            }
        }

        if (smallestSize > 0)
        {
            sizeSubSector = smallestSize;
            eraseSubSectorCommand = smallestCmd;
        }
        if (largestSize > sizeSubSector)
        {
            sizeSector = largestSize;
            eraseSectorCommand = largestCmd;
        }
        else
            sizeSector = sizeSubSector;

        if (totalBytes > 0 && sizeSubSector > 0)
            nrOfSubSectors = static_cast<uint32_t>(totalBytes / sizeSubSector);

        // DWORD 11: page size
        if (bfptTableLength >= 11)
        {
            const uint32_t dword11 = readDword(10);
            const uint8_t pageSizeExp = (dword11 >> 4) & 0x0F;
            if (pageSizeExp > 0)
                sizePage = 1u << pageSizeExp;
        }

        // DWORD 15: Quad Enable Requirements (bits [22:20]) — available from JESD216B (tableLength >= 14)
        if (bfptTableLength >= 14)
        {
            const uint32_t dword15 = readDword(14);
            qer = (dword15 >> 20) & 0x07;
        }
    }

    void FlashGeometryQuadSfdp::EnableQuadMode()
    {
        switch (qer)
        {
            case 0:
                sequencer.Continue();
                break;
            case 1:
            case 5:
                EnableQuad_Qer1_5();
                break;
            case 2:
                EnableQuad_Qer2();
                break;
            case 3:
                EnableQuad_Qer3();
                break;
            case 4:
                EnableQuad_Qer4();
                break;
            default:
                sequencer.Continue();
                break;
        }
    }

    void FlashGeometryQuadSfdp::EnableQuad_Qer1_5()
    {
        // Read SR2 (command 0x35), then WE, then write SR1+SR2 with SR2[1]=1
        static const hal::QuadSpi::Header readSr2Header{ std::make_optional(commandReadStatusRegister2), {}, {}, 0 };
        spi.ReceiveData(readSr2Header, infra::MakeByteRange(statusBuffer[1]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                static const hal::QuadSpi::Header readSr1Header{ std::make_optional(commandReadStatusRegister1), {}, {}, 0 };
                spi.ReceiveData(readSr1Header, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
                    {
                        static const hal::QuadSpi::Header writeEnableHeader{ std::make_optional(commandWriteEnable), {}, {}, 0 };
                        spi.SendData(writeEnableHeader, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
                            {
                                statusBuffer[1] |= 0x02; // set SR2[1] = QE
                                static const hal::QuadSpi::Header writeStatusHeader{ std::make_optional(commandWriteStatusRegister), {}, {}, 0 };
                                spi.SendData(writeStatusHeader, infra::MakeByteRange(statusBuffer), hal::QuadSpi::Lines::SingleSpeed(), [this]()
                                    {
                                        sequencer.Continue();
                                    });
                            });
                    });
            });
    }

    void FlashGeometryQuadSfdp::EnableQuad_Qer2()
    {
        // Read SR1, then WE, then write SR1 with SR1[6]=1
        static const hal::QuadSpi::Header readSr1Header{ std::make_optional(commandReadStatusRegister1), {}, {}, 0 };
        spi.ReceiveData(readSr1Header, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                static const hal::QuadSpi::Header writeEnableHeader{ std::make_optional(commandWriteEnable), {}, {}, 0 };
                spi.SendData(writeEnableHeader, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
                    {
                        statusBuffer[0] |= 0x40; // set SR1[6] = QE
                        static const hal::QuadSpi::Header writeStatusHeader{ std::make_optional(commandWriteStatusRegister), {}, {}, 0 };
                        spi.SendData(writeStatusHeader, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
                            {
                                sequencer.Continue();
                            });
                    });
            });
    }

    void FlashGeometryQuadSfdp::EnableQuad_Qer3()
    {
        // WE, then write SR2 with SR2[7]=1 via command 0x3E
        static const hal::QuadSpi::Header writeEnableHeader{ std::make_optional(commandWriteEnable), {}, {}, 0 };
        spi.SendData(writeEnableHeader, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                statusBuffer[0] = 0x80; // SR2[7] = QE
                static const hal::QuadSpi::Header writeStatusHeader{ std::make_optional(commandWriteStatusRegister2Alt), {}, {}, 0 };
                spi.SendData(writeStatusHeader, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
                    {
                        sequencer.Continue();
                    });
            });
    }

    void FlashGeometryQuadSfdp::EnableQuad_Qer4()
    {
        // Write SR2 with SR2[1]=1 via command 0x31, no WE needed
        statusBuffer[0] = 0x02; // SR2[1] = QE
        static const hal::QuadSpi::Header writeStatusHeader{ std::make_optional(commandWriteStatusRegister2), {}, {}, 0 };
        spi.SendData(writeStatusHeader, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                sequencer.Continue();
            });
    }
}
