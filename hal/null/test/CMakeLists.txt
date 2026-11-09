add_executable(hal.null_test)
emil_build_for(hal.null_test BOOL EMIL_BUILD_TESTS)
emil_add_test(hal.null_test)

target_link_libraries(hal.null_test PUBLIC
    gmock_main
    hal.null
    infra.util_test_helper
)

target_sources(hal.null_test PRIVATE
    TestAdcMultiChannelNull.cpp
    TestCanNull.cpp
    TestGpioPinNull.cpp
    TestSerialCommunicationNull.cpp
    TestSynchronousQuadratureEncoderNull.cpp
    TestThreeChannelsPwmNull.cpp
)
