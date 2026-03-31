#ifndef SERVICES_TRACING_ECHO_ON_SESAME_HPP
#define SERVICES_TRACING_ECHO_ON_SESAME_HPP

#include "services/echo_core/TracingEcho.hpp"
#include "services/echo/EchoOnSesame.hpp"

namespace services
{
    using TracingEchoOnSesame = TracingEchoOnStreamsDescendant<EchoOnSesame>;
}

#endif
