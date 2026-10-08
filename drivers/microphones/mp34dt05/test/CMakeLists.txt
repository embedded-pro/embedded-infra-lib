add_executable(drivers.microphones.mp34dt05_test)
emil_build_for(drivers.microphones.mp34dt05_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.microphones.mp34dt05_test)

target_link_libraries(drivers.microphones.mp34dt05_test PUBLIC
    gmock_main
    drivers.microphones.mp34dt05
    drivers.microphones.pdm_test_doubles
    hal.interfaces_test_doubles
    infra.event_test_helper
)

target_sources(drivers.microphones.mp34dt05_test PRIVATE
    TestMp34dt05.cpp
)
