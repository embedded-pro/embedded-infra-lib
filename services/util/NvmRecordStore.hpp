#pragma once

#include "infra/event/ClaimableResource.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/util/NvmRegion.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    class NvmRecordStore
    {
    public:
        template<std::size_t RegionSize>
        using WithRegionSize = infra::WithStorage<NvmRecordStore, std::array<uint8_t, 2 * RegionSize>>;

        static constexpr std::size_t headerSize{ 8 };

        enum class RestoreResult : uint8_t
        {
            pending,
            restored,
            defaults,
            corrupt
        };

        enum class SaveResult : uint8_t
        {
            none,
            pending,
            saved,
            failed
        };

        class Client
        {
        protected:
            Client() = default;
            Client(const Client& other) = delete;
            Client& operator=(const Client& other) = delete;
            ~Client() = default;

        public:
            virtual void Restored(infra::ConstByteRange payload) = 0;
            virtual std::size_t Capture(infra::ByteRange payload) = 0;
            virtual bool Busy() const = 0;
        };

        struct Config
        {
            Config();

            infra::Duration saveDelay{ std::chrono::seconds(2) };
        };

        NvmRecordStore(infra::ByteRange buffers, NvmRegion& first, NvmRegion& second, infra::ClaimableResource& access, Client& client, const Config& config = Config());
        NvmRecordStore(infra::ByteRange buffers, NvmRegion& first, NvmRegion& second, Client& client, const Config& config = Config());
        NvmRecordStore(const NvmRecordStore& other) = delete;
        NvmRecordStore& operator=(const NvmRecordStore& other) = delete;
        ~NvmRecordStore() = default;

        void ScheduleSave();
        RestoreResult Restore() const;
        SaveResult LastSave() const;

    private:
        struct Copy
        {
            uint16_t sequence;
            infra::ConstByteRange payload;
        };

        void ReadCopy(std::size_t copy);
        void CopyRead(infra::ConstByteRange data);
        void CopiesRead();
        std::optional<std::size_t> NewestCopy() const;
        void SaveWhenIdle();
        void Save();
        void WriteCopy(std::size_t length);
        void Saved(bool verified);

        infra::ClaimableResource ownAccess;
        infra::ClaimableResource::Claimer claimer;
        infra::ByteRange image;
        infra::ByteRange readBack;
        std::array<NvmRegion*, 2> regions;
        Client& client;
        Config config;
        std::array<std::optional<Copy>, 2> copies;
        std::array<bool, 2> erased{};
        uint16_t sequence{ 0 };
        uint16_t writing{ 0 };
        std::size_t reading{ 0 };
        std::size_t nextCopy{ 0 };
        bool saving{ false };
        bool dirty{ false };
        RestoreResult restore{ RestoreResult::pending };
        SaveResult lastSave{ SaveResult::none };
        infra::TimerSingleShot saveTimer;
    };
}
