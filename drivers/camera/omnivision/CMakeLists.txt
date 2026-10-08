add_library(drivers.camera.omnivision ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.camera.omnivision PUBLIC
    hal.interfaces
    infra.event
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.camera.omnivision PRIVATE
    OmniVisionSensor.cpp
    OmniVisionSensor.hpp
    RegisterTable.hpp
    RegisterTableRunner.cpp
    RegisterTableRunner.hpp
    SccbBusAccessI2c.cpp
    SccbBusAccessI2c.hpp
)

add_subdirectory(test)
