add_library(drivers.microphones.mp34dt05 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.microphones.mp34dt05 PUBLIC
    drivers.microphones.pdm
    hal.interfaces
    infra.event
    infra.util
)

target_sources(drivers.microphones.mp34dt05 PRIVATE
    Mp34dt05.cpp
    Mp34dt05.hpp
)

add_subdirectory(test)
