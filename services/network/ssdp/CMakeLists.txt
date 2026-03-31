add_library(services.network.ssdp ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.ssdp PUBLIC
    services.network.connection
    services.network.http
)

target_sources(services.network.ssdp PRIVATE
    SsdpDeviceDiscovery.cpp
    SsdpDeviceDiscovery.hpp
)

add_subdirectory(test)
