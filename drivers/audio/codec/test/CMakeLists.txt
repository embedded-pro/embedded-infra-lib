add_executable(drivers.audio.codec_test)
emil_build_for(drivers.audio.codec_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.audio.codec_test)

target_link_libraries(drivers.audio.codec_test PUBLIC
    gmock_main
    drivers.audio.codec
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.audio.codec_test PRIVATE
    TestCodecAudioOutput.cpp
)
