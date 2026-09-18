add_library(services.ble_profile_test_doubles INTERFACE)
emil_build_for(services.ble_profile_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.ble_profile_test_doubles INTERFACE
    services.ble_profile
    gmock
)

target_sources(services.ble_profile_test_doubles PRIVATE
    NordicUartMock.hpp
)
