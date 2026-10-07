add_library(drivers.audio.wm8994 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.audio.wm8994 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.audio.wm8994 PRIVATE
    Wm8994.cpp
    Wm8994.hpp
    Wm8994BusAccessI2c.cpp
    Wm8994BusAccessI2c.hpp
)

add_subdirectory(test)
