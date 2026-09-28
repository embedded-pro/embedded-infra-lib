#ifndef SERVICES_HIL_WATCH_DOG_COMMANDS_HPP
#define SERVICES_HIL_WATCH_DOG_COMMANDS_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>

namespace services
{
    class HilWatchDogFactory
    {
    protected:
        HilWatchDogFactory() = default;
        HilWatchDogFactory(const HilWatchDogFactory& other) = delete;
        HilWatchDogFactory& operator=(const HilWatchDogFactory& other) = delete;
        ~HilWatchDogFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> StartKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual HilStatus Create(uint8_t index, infra::Duration timeout, const HilArguments& arguments, hal::Watchdog*& watchDog) = 0;
    };

    class HilWatchDogCommands
        : public services::TerminalCommands
    {
    public:
        HilWatchDogCommands(HilContext& context, HilWatchDogFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Start(const HilArguments& arguments);
        HilStatus Feed(const HilArguments& arguments);

        HilStatus StartInstance(uint8_t index, infra::Duration timeout, bool feed, const HilArguments& arguments);
        HilStatus Parse(const HilArguments& arguments, uint8_t& index, uint32_t& timeout, bool& feed) const;
        void EarlyWarning();
        void Report();

    private:
        HilContext& context;
        HilWatchDogFactory& factory;
        HilSingleInstance instance;
        hal::Watchdog* watchDog = nullptr;
        bool autoFeed = true;
        std::atomic<uint32_t> warnings{ 0 };
        std::atomic<bool> reportPending{ false };
        std::array<Command, 2> commands;
    };
}

#endif
