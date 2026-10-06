add_executable(boards.boostxl_k350qvg_s1_test)
emil_build_for(boards.boostxl_k350qvg_s1_test BOOL EMIL_BUILD_TESTS)
emil_add_test(boards.boostxl_k350qvg_s1_test)

target_link_libraries(boards.boostxl_k350qvg_s1_test PUBLIC
    gmock_main
    boards.boostxl_k350qvg_s1
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(boards.boostxl_k350qvg_s1_test PRIVATE
    TestBoostxlK350qvgS1Setup.cpp
)
