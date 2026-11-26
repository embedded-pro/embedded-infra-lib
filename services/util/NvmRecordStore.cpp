#include "services/util/NvmRecordStore.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr uint16_t magic{ 0x5352 };
        constexpr uint8_t erasedByte{ 0xff };
        constexpr std::size_t sequenceOffset{ 2 };
        constexpr std::size_t lengthOffset{ 4 };
        constexpr std::size_t crcOffset{ 6 };

        uint16_t ReadUint16(infra::ConstByteRange data, std::size_t offset)
        {
            return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
        }

        void WriteUint16(infra::ByteRange data, std::size_t offset, uint16_t value)
        {
            data[offset] = static_cast<uint8_t>(value);
            data[offset + 1] = static_cast<uint8_t>(value >> 8);
        }

        uint16_t Crc(infra::ConstByteRange data, std::size_t length)
        {
            infra::Crc16CcittFalse crc;
            crc.Update(infra::Head(data, crcOffset));
            crc.Update(infra::Head(infra::DiscardHead(data, NvmRecordStore::headerSize), length));
            return crc.Result();
        }

        bool Newer(uint16_t candidate, uint16_t reference)
        {
            return static_cast<int16_t>(candidate - reference) > 0;
        }

        bool Erased(infra::ConstByteRange data)
        {
            return std::ranges::all_of(data, [](uint8_t byte)
                {
                    return byte == erasedByte;
                });
        }
    }

    NvmRecordStore::Config::Config() = default;

    NvmRecordStore::NvmRecordStore(infra::ByteRange buffers, NvmRegion& first, NvmRegion& second, infra::ClaimableResource& access, Client& client, const Config& config)
        : claimer(access)
        , image(infra::Head(buffers, buffers.size() / 2))
        , readBack(infra::DiscardHead(buffers, buffers.size() / 2))
        , regions{ &first, &second }
        , client(client)
        , config(config)
    {
        really_assert(buffers.size() % 2 == 0);
        really_assert(image.size() > headerSize);
        really_assert(image.size() - headerSize <= 0xffff);
        claimer.Claim([this]()
            {
                ReadCopy(0);
            });
    }

    NvmRecordStore::NvmRecordStore(infra::ByteRange buffers, NvmRegion& first, NvmRegion& second, Client& client, const Config& config)
        : NvmRecordStore(buffers, first, second, ownAccess, client, config)
    {}

    void NvmRecordStore::ScheduleSave()
    {
        lastSave = SaveResult::pending;
        saveTimer.Start(config.saveDelay, [this]()
            {
                SaveWhenIdle();
            });
    }

    NvmRecordStore::RestoreResult NvmRecordStore::Restore() const
    {
        return restore;
    }

    NvmRecordStore::SaveResult NvmRecordStore::LastSave() const
    {
        return lastSave;
    }

    void NvmRecordStore::ReadCopy(std::size_t copy)
    {
        reading = copy;
        regions[copy]->Read(copy == 0 ? image : readBack, [this]()
            {
                CopyRead(reading == 0 ? image : readBack);
            });
    }

    void NvmRecordStore::CopyRead(infra::ConstByteRange data)
    {
        erased[reading] = Erased(data);

        const auto length = ReadUint16(data, lengthOffset);
        if (ReadUint16(data, 0) == magic && length <= data.size() - headerSize && ReadUint16(data, crcOffset) == Crc(data, length))
            copies[reading] = Copy{ ReadUint16(data, sequenceOffset), infra::Head(infra::DiscardHead(data, headerSize), length) };

        if (reading == 0)
            ReadCopy(1);
        else
            CopiesRead();
    }

    void NvmRecordStore::CopiesRead()
    {
        claimer.Release();
        const auto newest = NewestCopy();

        if (newest.has_value())
        {
            sequence = copies[*newest]->sequence;
            nextCopy = 1 - *newest;
            restore = RestoreResult::restored;
            client.Restored(copies[*newest]->payload);
        }
        else
            restore = erased[0] && erased[1] ? RestoreResult::defaults : RestoreResult::corrupt;
    }

    std::optional<std::size_t> NvmRecordStore::NewestCopy() const
    {
        if (copies[0] && copies[1])
            return Newer(copies[1]->sequence, copies[0]->sequence) ? 1 : 0;
        if (copies[0])
            return 0;
        if (copies[1])
            return 1;
        return std::nullopt;
    }

    void NvmRecordStore::SaveWhenIdle()
    {
        if (saving)
            dirty = true;
        else if (restore == RestoreResult::pending || client.Busy())
            ScheduleSave();
        else
        {
            saving = true;
            claimer.Claim([this]()
                {
                    Save();
                });
        }
    }

    void NvmRecordStore::Save()
    {
        if (client.Busy())
        {
            claimer.Release();
            saving = false;
            dirty = false;
            ScheduleSave();
        }
        else
        {
            const auto length = client.Capture(infra::DiscardHead(image, headerSize));
            really_assert(length <= image.size() - headerSize);
            WriteCopy(length);
        }
    }

    void NvmRecordStore::WriteCopy(std::size_t length)
    {
        writing = static_cast<uint16_t>(sequence + 1);
        WriteUint16(image, 0, magic);
        WriteUint16(image, sequenceOffset, writing);
        WriteUint16(image, lengthOffset, static_cast<uint16_t>(length));
        WriteUint16(image, crcOffset, Crc(image, length));

        written = headerSize + length;
        regions[nextCopy]->Write(infra::Head(image, written), [this]()
            {
                regions[nextCopy]->Read(infra::Head(readBack, written), [this]()
                    {
                        Saved(std::ranges::equal(infra::Head(image, written), infra::Head(readBack, written)));
                    });
            });
    }

    void NvmRecordStore::Saved(bool verified)
    {
        claimer.Release();
        saving = false;
        if (!saveTimer.Armed())
            lastSave = verified ? SaveResult::saved : SaveResult::failed;

        if (verified)
        {
            sequence = writing;
            nextCopy = 1 - nextCopy;
        }

        if (dirty)
        {
            dirty = false;
            ScheduleSave();
        }
    }
}
