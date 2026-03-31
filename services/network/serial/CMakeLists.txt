add_library(services.network.serial ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.serial PUBLIC
    services.network.connection
)

target_sources(services.network.serial PRIVATE
    SerialServer.cpp
    SerialServer.hpp
)

add_subdirectory(test)
