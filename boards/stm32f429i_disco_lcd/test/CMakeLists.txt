add_executable(boards.stm32f429i_disco_lcd_test)
emil_build_for(boards.stm32f429i_disco_lcd_test BOOL EMIL_BUILD_TESTS)
emil_add_test(boards.stm32f429i_disco_lcd_test)

target_link_libraries(boards.stm32f429i_disco_lcd_test PUBLIC
    gmock_main
    boards.stm32f429i_disco_lcd
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(boards.stm32f429i_disco_lcd_test PRIVATE
    TestStm32f429iDiscoLcdSetup.cpp
)
