#include "bootloader/StatusJournal.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t numberOfRegions = 2;

        constexpr uint32_t Check(uint32_t marker, uint8_t status, uint32_t generation, uint32_t errorCode)
        {
            return ~(marker ^ (status | (generation << 8)) ^ errorCode);
        }

        bool IsErased(const StatusJournalEntry& entry)
        {
            const auto bytes = infra::MakeByteRange(entry);
            return std::ranges::all_of(bytes, [](uint8_t byte)
                {
                    return byte == 0xff;
                });
        }

        // The generation counts down, so a partial erase, which only sets bits, never makes an entry look newer
        bool IsAtLeastAsNew(const StatusJournalEntry& entry, const std::optional<StatusJournalEntry>& latest)
        {
            return !latest || GenerationOf(entry) <= GenerationOf(*latest);
        }
    }

    StatusJournalEntry MakeStatusJournalEntry(application::UpgradePackStatus status, uint32_t errorCode, uint32_t generation)
    {
        const auto statusByte = static_cast<uint8_t>(status);
        const std::array<uint8_t, 3> generationBytes{ static_cast<uint8_t>(generation), static_cast<uint8_t>(generation >> 8), static_cast<uint8_t>(generation >> 16) };
        return StatusJournalEntry{ StatusJournalEntry::validMarker, statusByte, generationBytes, errorCode, Check(StatusJournalEntry::validMarker, statusByte, generation & StatusJournalEntry::firstGeneration, errorCode) };
    }

    uint32_t GenerationOf(const StatusJournalEntry& entry)
    {
        return entry.generation[0] | (entry.generation[1] << 8) | (entry.generation[2] << 16);
    }

    bool IsValid(const StatusJournalEntry& entry)
    {
        return entry.marker == StatusJournalEntry::validMarker && entry.check == Check(entry.marker, entry.status, GenerationOf(entry), entry.errorCode);
    }

    StatusJournal::StatusJournal(hal::SynchronousFlash& flash)
        : flash(flash)
    {
        really_assert(flash.NumberOfSectors() >= numberOfRegions);
    }

    application::UpgradePackStatus StatusJournal::Status() const
    {
        const auto scan = Read();
        return scan.latest ? static_cast<application::UpgradePackStatus>(scan.latest->status) : application::UpgradePackStatus::empty;
    }

    uint32_t StatusJournal::ErrorCode() const
    {
        const auto scan = Read();
        return scan.latest ? scan.latest->errorCode : 0;
    }

    void StatusJournal::Record(application::UpgradePackStatus status, uint32_t errorCode)
    {
        const auto scan = Read();
        if (scan.latest && scan.latest->status == static_cast<uint8_t>(status) && scan.latest->errorCode == errorCode)
            return;

        if (scan.firstErased.has_value())
        {
            const auto entry = MakeStatusJournalEntry(status, errorCode, scan.generation);
            flash.WriteBuffer(infra::MakeByteRange(entry), *scan.firstErased);
        }
        else
            Rollover(scan, status, errorCode);
    }

    void StatusJournal::Clear()
    {
        const auto scan = Read();
        if (scan.latest && scan.latest->status != static_cast<uint8_t>(application::UpgradePackStatus::empty))
            Record(application::UpgradePackStatus::empty, 0);

        EraseCurrentLast(Read().current);
    }

    void StatusJournal::EraseCurrentLast(uint32_t current)
    {
        EraseRegion(1 - current);
        EraseRegion(current);
    }

    void StatusJournal::Rollover(const Scan& scan, application::UpgradePackStatus status, uint32_t errorCode)
    {
        if (scan.generation == 0)
        {
            EraseCurrentLast(scan.current);
            const auto entry = MakeStatusJournalEntry(status, errorCode, StatusJournalEntry::firstGeneration);
            flash.WriteBuffer(infra::MakeByteRange(entry), 0);
            return;
        }

        const auto other = 1 - scan.current;
        if (!scan.otherErased)
            EraseRegion(other);

        const auto entry = MakeStatusJournalEntry(status, errorCode, scan.generation - 1);
        flash.WriteBuffer(infra::MakeByteRange(entry), BeginOfRegion(other));
        EraseRegion(scan.current);
    }

    StatusJournal::Scan StatusJournal::Read() const
    {
        const std::array<RegionScan, numberOfRegions> regions{ ReadRegion(0), ReadRegion(1) };

        Scan scan;
        if (regions[1].latest && (!regions[0].latest || GenerationOf(*regions[1].latest) < GenerationOf(*regions[0].latest)))
            scan.current = 1;

        const auto& current = regions[scan.current];
        scan.latest = current.latest;
        if (current.tailErased)
            scan.firstErased = current.firstErased;
        scan.otherErased = regions[1 - scan.current].erased;
        if (current.latest)
            scan.generation = GenerationOf(*current.latest);

        return scan;
    }

    StatusJournal::RegionScan StatusJournal::ReadRegion(uint32_t region) const
    {
        RegionScan scan;

        for (auto address = BeginOfRegion(region); address + sizeof(StatusJournalEntry) <= EndOfRegion(region); address += sizeof(StatusJournalEntry))
        {
            const auto entry = ReadEntry(address);
            if (IsErased(entry))
            {
                if (!scan.firstErased.has_value())
                    scan.firstErased = address;
                continue;
            }

            scan.erased = false;
            if (scan.firstErased.has_value())
                scan.tailErased = false;
            if (!scan.firstErased.has_value() && IsValid(entry) && IsAtLeastAsNew(entry, scan.latest))
                scan.latest = entry;
        }

        return scan;
    }

    StatusJournalEntry StatusJournal::ReadEntry(uint32_t address) const
    {
        StatusJournalEntry entry{};
        flash.ReadBuffer(infra::MakeByteRange(entry), address);
        return entry;
    }

    uint32_t StatusJournal::BeginOfRegion(uint32_t region) const
    {
        return flash.AddressOfSector(region * flash.NumberOfSectors() / numberOfRegions);
    }

    uint32_t StatusJournal::EndOfRegion(uint32_t region) const
    {
        return region + 1 == numberOfRegions ? flash.TotalSize() : BeginOfRegion(region + 1);
    }

    void StatusJournal::EraseRegion(uint32_t region)
    {
        const auto sectors = flash.NumberOfSectors();
        flash.EraseSectors(region * sectors / numberOfRegions, (region + 1) * sectors / numberOfRegions);
    }
}
