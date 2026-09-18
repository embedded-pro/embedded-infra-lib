add_library(drivers.imu.l3gd20 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.l3gd20 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.imu.l3gd20 PRIVATE
    L3gd20BusAccessI2c.cpp
    L3gd20BusAccessI2c.hpp
    L3gd20BusAccessSpi.cpp
    L3gd20BusAccessSpi.hpp
    L3gd20Core.cpp
    L3gd20Core.hpp
)

add_subdirectory(test)
