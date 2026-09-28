#ifndef SERVICES_HIL_SINGLE_INSTANCE_GROUP_HPP
#define SERVICES_HIL_SINGLE_INSTANCE_GROUP_HPP

#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <cstdint>

namespace services
{
    class HilInstanceFactory
    {
    protected:
        HilInstanceFactory() = default;
        HilInstanceFactory(const HilInstanceFactory& other) = delete;
        HilInstanceFactory& operator=(const HilInstanceFactory& other) = delete;
        ~HilInstanceFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class HilSingleInstanceGroup
        : public services::TerminalCommands
    {
    protected:
        HilSingleInstanceGroup(HilContext& context, HilInstanceFactory& factory, HilOwner owner);
        HilSingleInstanceGroup(const HilSingleInstanceGroup& other) = delete;
        HilSingleInstanceGroup& operator=(const HilSingleInstanceGroup& other) = delete;
        ~HilSingleInstanceGroup() = default;

        Command OpenCommand(const char* name, const char* usage);
        Command CloseCommand(const char* name, const char* usage);

        virtual HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) = 0;
        virtual void Opened(HilResponse::Line& line) const;
        virtual void CloseInstance() = 0;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);
        void Closed();

    protected:
        HilContext& context;
        HilSingleInstance instance;
        HilPinOwner pins;

    private:
        HilInstanceFactory& factory;
    };
}

#endif
