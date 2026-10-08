add_executable(boards.stm32f4_disco_audio_test)
emil_build_for(boards.stm32f4_disco_audio_test BOOL EMIL_BUILD_TESTS)
emil_add_test(boards.stm32f4_disco_audio_test)

target_link_libraries(boards.stm32f4_disco_audio_test PUBLIC
    gmock_main
    boards.stm32f4_disco_audio
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(boards.stm32f4_disco_audio_test PRIVATE
    TestStm32f4DiscoAudioSetup.cpp
)
