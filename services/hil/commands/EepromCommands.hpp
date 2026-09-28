#ifndef SERVICES_HIL_EEPROM_COMMANDS_HPP
#define SERVICES_HIL_EEPROM_COMMANDS_HPP

#include "hal/interfaces/Eeprom.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services::hil
{
    class EepromFactory
    {
    protected:
        EepromFactory() = default;
        EepromFactory(const EepromFactory& other) = delete;
        EepromFactory& operator=(const EepromFactory& other) = delete;
        ~EepromFactory() = default;

    public:
        virtual hal::Eeprom& Instance() = 0;
    };

    class EepromCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<EepromCommands, std::array<uint8_t, Capacity>>;

        EepromCommands(infra::ByteRange buffer, Context& context, EepromFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Write(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Erase(const Arguments& arguments);

        infra::Function<void()> Start();
        void Done();
        void Timeout();

    private:
        infra::ByteRange buffer;
        Context& context;
        EepromFactory& factory;
        infra::ByteRange readData;
        bool operating = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
