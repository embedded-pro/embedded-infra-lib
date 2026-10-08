add_executable(drivers.microphones.pdm_test)
emil_build_for(drivers.microphones.pdm_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.microphones.pdm_test)

target_link_libraries(drivers.microphones.pdm_test PUBLIC
    gmock_main
    drivers.microphones.pdm
    drivers.microphones.pdm_test_doubles
    hal.interfaces_test_doubles
)

target_sources(drivers.microphones.pdm_test PRIVATE
    TestPdmMicrophone.cpp
)
