#include "services/flash/FlashGeometrySfdp.hpp"

namespace services
{
    FlashGeometrySfdp::FlashGeometrySfdp(hal::SpiMaster& spi, infra::Function<void()> onInitialized)
        : FlashGeometrySfdpBase(onInitialized)
        , spi(spi)
    {}

    void FlashGeometrySfdp::PerformRead(uint32_t address, infra::ByteRange buffer, infra::Function<void()> onDone)
    {
        pendingBuffer = buffer;
        pendingOnDone = onDone;
        commandBuffer = {
            commandReadSfdp,
            static_cast<uint8_t>(address >> 16),
            static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address),
            0xFF
        };
        spi.SendData(infra::MakeByteRange(commandBuffer), hal::SpiAction::continueSession, [this]()
            {
                spi.ReceiveData(pendingBuffer, hal::SpiAction::stop, pendingOnDone);
            });
    }
}
