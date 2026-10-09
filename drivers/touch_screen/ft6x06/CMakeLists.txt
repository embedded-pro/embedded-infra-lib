add_library(drivers.touch_screen.ft6x06 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.touch_screen.ft6x06 PUBLIC
    hal.interfaces
    infra.event
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.touch_screen.ft6x06 PRIVATE
    Ft6x06.cpp
    Ft6x06.hpp
    Ft6x06BusAccessI2c.cpp
    Ft6x06BusAccessI2c.hpp
)

add_subdirectory(test)
