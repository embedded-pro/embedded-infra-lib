add_library(drivers.imu.lis3dsh ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.lis3dsh PUBLIC
    drivers.imu.common
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.imu.lis3dsh PRIVATE
    Lis3dshBusAccess.hpp
    Lis3dshCore.cpp
    Lis3dshCore.hpp
)

add_subdirectory(test)
