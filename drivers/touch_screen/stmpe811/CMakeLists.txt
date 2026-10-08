add_library(drivers.touch_screen.stmpe811 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.touch_screen.stmpe811 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.touch_screen.stmpe811 PRIVATE
    Stmpe811.cpp
    Stmpe811.hpp
    Stmpe811BusAccessI2c.cpp
    Stmpe811BusAccessI2c.hpp
)

add_subdirectory(test)
