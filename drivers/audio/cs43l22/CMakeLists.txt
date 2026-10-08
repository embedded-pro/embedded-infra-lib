add_library(drivers.audio.cs43l22 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.audio.cs43l22 PUBLIC
    hal.interfaces
    infra.util
    services.util.register_access
)

target_sources(drivers.audio.cs43l22 PRIVATE
    Cs43l22BusAccessI2c.cpp
    Cs43l22BusAccessI2c.hpp
)

add_subdirectory(test)
