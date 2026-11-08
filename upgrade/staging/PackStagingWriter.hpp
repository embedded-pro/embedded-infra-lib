#ifndef UPGRADE_STAGING_PACK_STAGING_WRITER_HPP
#define UPGRADE_STAGING_PACK_STAGING_WRITER_HPP

#include "hal/interfaces/Flash.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <cstdint>

namespace application
{
    class PackStagingWriter
    {
    public:
        static constexpr uint16_t maxPageSize{ 256 };
        static constexpr uint8_t maxAppendSize{ 8 };

        PackStagingWriter(hal::Flash& flash, uint16_t pageSize, infra::ByteRange scratch, const infra::Function<bool()>& proceed);
        PackStagingWriter(const PackStagingWriter&) = delete;
        PackStagingWriter& operator=(const PackStagingWriter&) = delete;
        PackStagingWriter(PackStagingWriter&&) = delete;
        PackStagingWriter& operator=(PackStagingWriter&&) = delete;

        void Erase(const infra::Function<void(bool)>& onErased);
        void Append(infra::ConstByteRange data, const infra::Function<void(bool)>& onAppended);
        void Flush(const infra::Function<void(bool)>& onFlushed);

        bool Busy() const;
        uint32_t Written() const;
        bool HoldsPartialPage() const;

    private:
        void EraseNextSector();
        void OnSectorErased();
        void CheckErasedChunk();
        void OnErasedChunk(uint32_t length);
        void ProgramPage();
        void OnPageWritten();
        void OnPageProgrammed();
        bool StepDone();
        void Finish(bool ok);

        hal::Flash& flash;
        uint16_t pageSize;
        infra::ByteRange scratch;
        infra::Function<bool()> proceed;
        infra::Function<void(bool)> onDone;
        bool busy{ false };

        uint32_t eraseSector{ 0 };
        uint32_t readAddress{ 0 };
        uint32_t readEnd{ 0 };
        uint32_t written{ 0 };
        uint32_t pageAddress{ 0 };
        uint16_t pageFill{ 0 };
        std::array<uint8_t, maxPageSize> page{};
        std::array<uint8_t, maxAppendSize> carry{};
        uint8_t carryFill{ 0 };
    };
}

#endif
