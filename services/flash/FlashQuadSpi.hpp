#ifndef SERVICES_FLASH_QUAD_SPI_HPP
#define SERVICES_FLASH_QUAD_SPI_HPP

#include "hal/interfaces/FlashHomogeneous.hpp"
#include "hal/interfaces/QuadSpi.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Sequencer.hpp"
#include "services/flash/FlashGeometry.hpp"

namespace services
{
    class FlashQuadSpi
        : public hal::FlashHomogeneous
    {
    public:
        FlashQuadSpi(hal::QuadSpi& spi, FlashGeometry& geometry);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone) override;

    protected:
        void WriteBufferSequence();
        infra::BoundedVector<uint8_t>::WithMaxSize<4> ConvertAddress(uint32_t address) const;
        void ScheduleOnDone();

        virtual void PageProgram() = 0;
        virtual void WriteEnable() = 0;
        virtual void EraseSomeSectors(uint32_t endIndex) = 0;
        virtual void HoldWhileWriteInProgress() = 0;

    protected:
        hal::QuadSpi& spi;
        infra::Sequencer sequencer;
        infra::AutoResetFunction<void()> onDone;
        infra::ConstByteRange buffer;
        uint32_t address = 0;
        uint32_t sectorIndex = 0;
    };
}

#endif
