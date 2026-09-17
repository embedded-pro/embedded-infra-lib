add_library(drivers.imu.lsm303dlhc ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.lsm303dlhc PUBLIC
    hal.interfaces
    infra.timer
    infra.util
)

target_sources(drivers.imu.lsm303dlhc PRIVATE
    Lsm303dlhc.hpp
    Lsm303dlhcAccelerometer.cpp
    Lsm303dlhcAccelerometer.hpp
    Lsm303dlhcAccelerometerWithFifo.hpp
    Lsm303dlhcBusAccessI2c.cpp
    Lsm303dlhcBusAccessI2c.hpp
    Lsm303dlhcMagnetometer.cpp
    Lsm303dlhcMagnetometer.hpp
    Lsm303dlhcSensor.cpp
    Lsm303dlhcSensor.hpp
    Lsm303dlhcWithPolling.hpp
)

add_subdirectory(test)
