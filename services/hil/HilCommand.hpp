#ifndef SERVICES_HIL_COMMAND_HPP
#define SERVICES_HIL_COMMAND_HPP

#include "services/hil/HilArguments.hpp"
#include "services/hil/HilPinNaming.hpp"
#include "services/hil/HilPinPool.hpp"
#include "services/hil/HilResponse.hpp"
#include "services/hil/HilStatus.hpp"
#include "services/util/Terminal.hpp"
#include <type_traits>
#include <variant>

namespace services
{
    struct HilContext
    {
        HilResponse& response;
        HilPinPool& pins;
        const HilPinNaming& naming;
        services::TerminalWithCommands& terminal;
    };

    template<class Group, HilStatus (Group::*Method)(const HilArguments&)>
    services::TerminalCommands::Command HilBind(const char* name, const char* usage, Group& group, HilResponse& response)
    {
        return { { name, name, "", usage }, [&group, &response](const infra::BoundedConstString& parameters)
            {
                HilStatus status = (group.*Method)(HilArguments(parameters));

                if (status != HilStatus::done)
                    response.Error(status);
            } };
    }

    template<class Group, HilStatus (Group::*Method)(const HilArguments&) const>
    services::TerminalCommands::Command HilBind(const char* name, const char* usage, const Group& group, HilResponse& response)
    {
        return { { name, name, "", usage }, [&group, &response](const infra::BoundedConstString& parameters)
            {
                HilStatus status = (group.*Method)(HilArguments(parameters));

                if (status != HilStatus::done)
                    response.Error(status);
            } };
    }

    template<class... Drivers, class F>
    void HilWithDriver(std::variant<std::monostate, Drivers...>& driver, F&& f)
    {
        std::visit([&f]<class Alternative>(Alternative& alternative)
            {
                if constexpr (!std::is_same_v<Alternative, std::monostate>)
                    f(alternative);
            },
            driver);
    }
}

#endif
