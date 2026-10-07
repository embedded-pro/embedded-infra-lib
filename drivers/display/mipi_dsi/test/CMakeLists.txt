add_executable(drivers.display.mipi_dsi_test)
emil_build_for(drivers.display.mipi_dsi_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.display.mipi_dsi_test)

target_link_libraries(drivers.display.mipi_dsi_test PUBLIC
    gmock_main
    drivers.display.mipi_dsi
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.display.mipi_dsi_test PRIVATE
    MipiDsiTestPanel.hpp
    TestMipiDsiDisplay.cpp
    TestMipiDsiPanelCore.cpp
    TestMipiDsiVideoPanel.cpp
)
