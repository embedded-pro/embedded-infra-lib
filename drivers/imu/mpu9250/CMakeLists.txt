add_library(drivers.imu.mpu9250 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.mpu9250 PUBLIC
    hal.interfaces
    infra.timer
    infra.util
)

target_sources(drivers.imu.mpu9250 PRIVATE
    Mpu9250BusAccess.hpp
    Mpu9250BusAccessI2c.cpp
    Mpu9250BusAccessI2c.hpp
    Mpu9250BusAccessSpi.cpp
    Mpu9250BusAccessSpi.hpp
    Mpu9250Core.cpp
    Mpu9250Core.hpp
)

add_subdirectory(test)
