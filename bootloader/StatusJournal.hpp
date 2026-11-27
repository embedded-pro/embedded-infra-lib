#ifndef BOOTLOADER_STATUS_JOURNAL_HPP
#define BOOTLOADER_STATUS_JOURNAL_HPP

#include "hal/synchronous_interfaces/SynchronousFlash.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace bootloader
{
    struct StatusJournalEntry
    {
        static constexpr uint32_t validMarker = 0x314a4d45;
        static constexpr uint32_t firstGeneration = 0xffffff;

        uint32_t marker;
        uint8_t status;
        std::array<uint8_t, 3> generation;
        uint32_t errorCode;
        uint32_t check;
    };

    static_assert(sizeof(StatusJournalEntry) == 16);

    StatusJournalEntry MakeStatusJournalEntry(application::UpgradePackStatus status, uint32_t errorCode, uint32_t generation);
    uint32_t GenerationOf(const StatusJournalEntry& entry);
    bool IsValid(const StatusJournalEntry& entry);

    class StatusJournal
    {
    public:
        explicit StatusJournal(hal::SynchronousFlash& flash);

        application::UpgradePackStatus Status() const;
        uint32_t ErrorCode() const;

        void Record(application::UpgradePackStatus status, uint32_t errorCode);
        void Clear();

    private:
        struct RegionScan
        {
            std::optional<StatusJournalEntry> latest;
            std::optional<uint32_t> firstErased;
            bool erased = true;
        };

        struct Scan
        {
            std::optional<StatusJournalEntry> latest;
            std::optional<uint32_t> firstErased;
            uint32_t generation = StatusJournalEntry::firstGeneration;
            uint32_t current = 0;
            bool otherErased = true;
        };

        Scan Read() const;
        RegionScan ReadRegion(uint32_t region) const;
        StatusJournalEntry ReadEntry(uint32_t address) const;
        void Rollover(const Scan& scan, application::UpgradePackStatus status, uint32_t errorCode);
        uint32_t BeginOfRegion(uint32_t region) const;
        uint32_t EndOfRegion(uint32_t region) const;
        void EraseRegion(uint32_t region);

        hal::SynchronousFlash& flash;
    };
}

#endif
