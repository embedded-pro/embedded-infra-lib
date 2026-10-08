add_library(drivers.imu.iis2dlpc ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.imu.iis2dlpc PUBLIC
    drivers.imu.common
    hal.interfaces
    infra.timer
    infra.util
    services.util.register_access
)

target_sources(drivers.imu.iis2dlpc PRIVATE
    Iis2dlpcBusAccess.hpp
    Iis2dlpcCore.cpp
    Iis2dlpcCore.hpp
)

add_subdirectory(test)
