add_library(services.util.register_access ${EMIL_EXCLUDE_FROM_ALL} STATIC)

# Kept apart from services.util so that drivers can sequence register access without
# also depending on the tracer, syntax and crypto stacks that services.util pulls in
target_link_libraries(services.util.register_access PUBLIC
    infra.timer
    infra.util
)

target_sources(services.util.register_access PRIVATE
    RegisterBusAccess.hpp
    RegisterStepRunner.cpp
    RegisterStepRunner.hpp
)

add_library(services.util.watchdog ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.util.watchdog PUBLIC
    hal.interfaces
    infra.timer
)

target_sources(services.util.watchdog PRIVATE
    EventLoopWatchdog.cpp
    EventLoopWatchdog.hpp
)

add_library(services.util ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.util PUBLIC
    hal.interfaces
    infra.syntax
    infra.timer
    services.crypto
    services.tracer
)

target_sources(services.util PRIVATE
    ConfigurationStore.cpp
    ConfigurationStore.hpp
    CyclicStore.cpp
    CyclicStore.hpp
    Stoppable.hpp
    Terminal.cpp
    Terminal.hpp
    TerminalWithStorage.cpp
    TerminalWithStorage.hpp
    TimeWithLocalization.cpp
    TimeWithLocalization.hpp
    WindowedProtocolTypes.hpp
    WritableConfiguration.hpp
)

if (EMIL_HOST_BUILD)
    target_link_libraries(services.util PUBLIC
        hal.generic
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
