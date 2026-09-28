add_library(services.hil ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.hil PUBLIC
    hal.interfaces
    hal.synchronous_interfaces
    infra.timer
    infra.util
    services.tracer
    services.util
)

target_sources(services.hil PRIVATE
    Arguments.cpp
    Arguments.hpp
    BoardInfo.hpp
    Command.hpp
    DeadlineTimeKeeper.cpp
    DeadlineTimeKeeper.hpp
    HilTerminal.cpp
    HilTerminal.hpp
    PinId.hpp
    PinNaming.cpp
    PinNaming.hpp
    PinPool.cpp
    PinPool.hpp
    Response.cpp
    Response.hpp
    Status.cpp
    Status.hpp
    SystemCommands.cpp
    SystemCommands.hpp
)

add_subdirectory(commands)
add_subdirectory(test)
