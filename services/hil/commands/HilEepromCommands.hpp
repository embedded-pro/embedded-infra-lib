#ifndef SERVICES_HIL_EEPROM_COMMANDS_HPP
#define SERVICES_HIL_EEPROM_COMMANDS_HPP

#include "hal/interfaces/Eeprom.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services
{
    class HilEepromFactory
    {
    protected:
        HilEepromFactory() = default;
        HilEepromFactory(const HilEepromFactory& other) = delete;
        HilEepromFactory& operator=(const HilEepromFactory& other) = delete;
        ~HilEepromFactory() = default;

    public:
        virtual hal::Eeprom& Instance() = 0;
    };

    class HilEepromCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<HilEepromCommands, std::array<uint8_t, Capacity>>;

        HilEepromCommands(infra::ByteRange buffer, HilContext& context, HilEepromFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Write(const HilArguments& arguments);
        HilStatus Read(const HilArguments& arguments);
        HilStatus Erase(const HilArguments& arguments);

        infra::Function<void()> Start();
        void Done();
        void Timeout();

    private:
        infra::ByteRange buffer;
        HilContext& context;
        HilEepromFactory& factory;
        infra::ByteRange readData;
        bool operating = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
