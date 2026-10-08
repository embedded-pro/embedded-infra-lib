add_library(drivers.microphones.mp45dt02 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.microphones.mp45dt02 PUBLIC
    drivers.microphones.pdm
    hal.interfaces
    infra.util
)

target_sources(drivers.microphones.mp45dt02 PRIVATE
    Mp45dt02.cpp
    Mp45dt02.hpp
)

add_subdirectory(test)
