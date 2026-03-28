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
    WritableConfiguration.hpp
)

if (EMIL_HOST_BUILD)
    target_link_libraries(services.util PUBLIC
        hal.generic
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
