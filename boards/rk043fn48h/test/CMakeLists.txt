add_executable(boards.rk043fn48h_test)
emil_build_for(boards.rk043fn48h_test BOOL EMIL_BUILD_TESTS)
emil_add_test(boards.rk043fn48h_test)

target_link_libraries(boards.rk043fn48h_test PUBLIC
    gmock_main
    boards.rk043fn48h
)

target_sources(boards.rk043fn48h_test PRIVATE
    TestRk043fn48h.cpp
)
