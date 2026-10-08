add_executable(drivers.microphones.mp45dt02_test)
emil_build_for(drivers.microphones.mp45dt02_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.microphones.mp45dt02_test)

target_link_libraries(drivers.microphones.mp45dt02_test PUBLIC
    gmock_main
    drivers.microphones.mp45dt02
    drivers.microphones.pdm_test_doubles
    hal.interfaces_test_doubles
)

target_sources(drivers.microphones.mp45dt02_test PRIVATE
    TestMp45dt02.cpp
)
