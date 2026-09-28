#ifndef SERVICES_HIL_COMMAND_HPP
#define SERVICES_HIL_COMMAND_HPP

#include "services/hil/Arguments.hpp"
#include "services/hil/PinNaming.hpp"
#include "services/hil/PinPool.hpp"
#include "services/hil/Response.hpp"
#include "services/hil/Status.hpp"
#include "services/util/Terminal.hpp"
#include <type_traits>
#include <variant>

namespace services::hil
{
    struct Context
    {
        Response& response;
        PinPool& pins;
        const PinNaming& naming;
        services::TerminalWithCommands& terminal;
    };

    template<class Group, Status (Group::*Method)(const Arguments&)>
    services::TerminalCommands::Command Bind(const char* name, const char* usage, Group& group, Response& response)
    {
        return { { name, name, usage }, [&group, &response](const infra::BoundedConstString& parameters)
            {
                Status status = (group.*Method)(Arguments(parameters));

                if (status != Status::done)
                    response.Error(status);
            } };
    }

    template<class... Drivers, class F>
    void WithDriver(std::variant<std::monostate, Drivers...>& driver, F&& f)
    {
        std::visit([&f](auto& alternative)
            {
                if constexpr (!std::is_same_v<std::decay_t<decltype(alternative)>, std::monostate>)
                    f(alternative);
            },
            driver);
    }
}

#endif
