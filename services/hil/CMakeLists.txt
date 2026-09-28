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
    HilArguments.cpp
    HilArguments.hpp
    HilBoardInfo.hpp
    HilCommand.hpp
    HilDeadlineTimeKeeper.cpp
    HilDeadlineTimeKeeper.hpp
    HilPinId.hpp
    HilPinNaming.cpp
    HilPinNaming.hpp
    HilPinPool.cpp
    HilPinPool.hpp
    HilResponse.cpp
    HilResponse.hpp
    HilStatus.cpp
    HilStatus.hpp
    HilSystemCommands.cpp
    HilSystemCommands.hpp
    HilTerminal.cpp
    HilTerminal.hpp
)

add_subdirectory(commands)
add_subdirectory(test)
