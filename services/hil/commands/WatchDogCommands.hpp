#ifndef SERVICES_HIL_WATCH_DOG_COMMANDS_HPP
#define SERVICES_HIL_WATCH_DOG_COMMANDS_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>

namespace services::hil
{
    class WatchDogFactory
    {
    protected:
        WatchDogFactory() = default;
        WatchDogFactory(const WatchDogFactory& other) = delete;
        WatchDogFactory& operator=(const WatchDogFactory& other) = delete;
        ~WatchDogFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> StartKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Create(uint8_t index, infra::Duration timeout, const Arguments& arguments, hal::Watchdog*& watchDog) = 0;
    };

    class WatchDogCommands
        : public services::TerminalCommands
    {
    public:
        WatchDogCommands(Context& context, WatchDogFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Start(const Arguments& arguments);
        Status Feed(const Arguments& arguments);

        Status StartInstance(uint8_t index, infra::Duration timeout, bool feed, const Arguments& arguments);
        Status Parse(const Arguments& arguments, uint8_t& index, uint32_t& timeout, bool& feed) const;
        void EarlyWarning();
        void Report();

    private:
        Context& context;
        WatchDogFactory& factory;
        SingleInstance instance;
        hal::Watchdog* watchDog = nullptr;
        bool autoFeed = true;
        std::atomic<uint32_t> warnings{ 0 };
        std::atomic<bool> reportPending{ false };
        std::array<Command, 2> commands;
    };
}

#endif
