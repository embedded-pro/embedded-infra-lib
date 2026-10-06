add_library(drivers.display.ssd2119 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.display.ssd2119 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.display.ssd2119 PRIVATE
    Ssd2119.cpp
    Ssd2119.hpp
    Ssd2119BoostxlK350qvgS1.cpp
    Ssd2119BoostxlK350qvgS1.hpp
    Ssd2119BusAccessSpi.cpp
    Ssd2119BusAccessSpi.hpp
)

add_subdirectory(test)
