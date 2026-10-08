add_library(drivers.imu.lis302dl ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.lis302dl PUBLIC
    drivers.imu.common
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.imu.lis302dl PRIVATE
    Lis302dlBusAccess.hpp
    Lis302dlCore.cpp
    Lis302dlCore.hpp
)

add_subdirectory(test)
