add_library(drivers.imu.common ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.common PUBLIC
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.imu.common PRIVATE
    AccelerometerSensor.cpp
    AccelerometerSensor.hpp
    ImuBusAccessI2c.cpp
    ImuBusAccessI2c.hpp
    ImuBusAccessSpi.cpp
    ImuBusAccessSpi.hpp
    RegisterSensor.cpp
    RegisterSensor.hpp
    SensorWithPolling.hpp
)

add_subdirectory(test)
