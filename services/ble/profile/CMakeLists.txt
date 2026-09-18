add_library(services.ble_profile ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.ble_profile PUBLIC
    hal.interfaces
    infra.event
    infra.util
    services.ble
)

target_sources(services.ble_profile PRIVATE
    NordicUart.hpp
    NordicUartCentral.cpp
    NordicUartCentral.hpp
    NordicUartPeripheral.cpp
    NordicUartPeripheral.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
