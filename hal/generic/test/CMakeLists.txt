add_executable(hal.generic_test)
emil_build_for(hal.generic_test HOST All PREREQUISITE_BOOL EMIL_BUILD_TESTS EMIL_STANDALONE)
emil_add_test(hal.generic_test)

target_link_libraries(hal.generic_test PUBLIC
    gmock_main
    hal.generic
    infra.event_test_helper
)

target_sources(hal.generic_test PRIVATE
    TestTimerServiceGeneric.cpp
)
