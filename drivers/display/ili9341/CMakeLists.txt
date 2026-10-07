add_library(drivers.display.ili9341 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.display.ili9341 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.display.ili9341 PRIVATE
    Ili9341.cpp
    Ili9341.hpp
    Ili9341BusAccessSpi.cpp
    Ili9341BusAccessSpi.hpp
)

add_subdirectory(test)
