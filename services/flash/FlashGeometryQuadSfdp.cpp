#include "services/flash/FlashGeometryQuadSfdp.hpp"

namespace services
{
    namespace
    {
        constexpr uint8_t commandWriteEnable = 0x06;
        constexpr uint8_t commandReadSr1 = 0x05;
        constexpr uint8_t commandReadSr2 = 0x35;
        constexpr uint8_t commandWriteSr = 0x01;
        constexpr uint8_t commandWriteSr2 = 0x31;
        constexpr uint8_t commandWriteSr2Alt = 0x3E;
    }

    FlashGeometryQuadSfdp::FlashGeometryQuadSfdp(hal::QuadSpi& spi, infra::Function<void()> onInitialized)
        : FlashGeometrySfdpBase(onInitialized)
        , spi(spi)
    {}

    uint8_t FlashGeometryQuadSfdp::EraseSubSectorCommand() const { return eraseSubSectorCommand; }

    uint8_t FlashGeometryQuadSfdp::EraseSectorCommand() const { return eraseSectorCommand; }

    uint8_t FlashGeometryQuadSfdp::EraseBulkCommand() const { return commandEraseBulk; }

    uint8_t FlashGeometryQuadSfdp::PageProgramCommand() const { return commandPageProgram; }

    uint8_t FlashGeometryQuadSfdp::ReadDataCommand() const { return readDataCommand; }

    uint8_t FlashGeometryQuadSfdp::ReadDummyCycles() const { return readDummyCycles; }

    void FlashGeometryQuadSfdp::PerformRead(uint32_t address, infra::ByteRange buffer, infra::Function<void()> onDone)
    {
        sfdpAddressVector = hal::QuadSpi::AddressToVector(address, 3);
        const hal::QuadSpi::Header header{ std::make_optional(commandReadSfdp), sfdpAddressVector, {}, 8 };
        spi.ReceiveData(header, buffer, hal::QuadSpi::Lines::SingleSpeed(), onDone);
    }

    void FlashGeometryQuadSfdp::OnBfptParsed(infra::Function<void()> onDone)
    {
        onQuadDone = onDone;
        switch (qer)
        {
            case 1:
            case 5:
                QerTransition(QerReadSr2{});
                break;
            case 2:
                QerTransition(QerReadSr1Only{});
                break;
            case 3:
                QerTransition(QerWriteEnableForAlt{});
                break;
            case 4:
                QerTransition(QerWriteSr2{});
                break;
            default:
                onDone();
                break;
        }
    }

    void FlashGeometryQuadSfdp::QerTransition(QerState newState)
    {
        qerState = newState;
        std::visit([this](auto& s)
            {
                Handle(s);
            },
            qerState);
    }

    void FlashGeometryQuadSfdp::Handle(QerReadSr2&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandReadSr2), {}, {}, 0 };
        spi.ReceiveData(h, infra::MakeByteRange(statusBuffer[1]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerReadSr1{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerReadSr1&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandReadSr1), {}, {}, 0 };
        spi.ReceiveData(h, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerWriteEnableFor12{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteEnableFor12&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteEnable), {}, {}, 0 };
        spi.SendData(h, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerWriteSr12{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteSr12&)
    {
        statusBuffer[1] |= 0x02;
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteSr), {}, {}, 0 };
        spi.SendData(h, infra::MakeByteRange(statusBuffer), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                onQuadDone();
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerReadSr1Only&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandReadSr1), {}, {}, 0 };
        spi.ReceiveData(h, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerWriteEnableFor1{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteEnableFor1&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteEnable), {}, {}, 0 };
        spi.SendData(h, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerWriteSr1{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteSr1&)
    {
        statusBuffer[0] |= 0x40;
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteSr), {}, {}, 0 };
        spi.SendData(h, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                onQuadDone();
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteEnableForAlt&)
    {
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteEnable), {}, {}, 0 };
        spi.SendData(h, {}, hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                QerTransition(QerWriteSr2Alt{});
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteSr2Alt&)
    {
        statusBuffer[0] = 0x80;
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteSr2Alt), {}, {}, 0 };
        spi.SendData(h, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                onQuadDone();
            });
    }

    void FlashGeometryQuadSfdp::Handle(QerWriteSr2&)
    {
        statusBuffer[0] = 0x02;
        static const hal::QuadSpi::Header h{ std::make_optional(commandWriteSr2), {}, {}, 0 };
        spi.SendData(h, infra::MakeByteRange(statusBuffer[0]), hal::QuadSpi::Lines::SingleSpeed(), [this]()
            {
                onQuadDone();
            });
    }
}
