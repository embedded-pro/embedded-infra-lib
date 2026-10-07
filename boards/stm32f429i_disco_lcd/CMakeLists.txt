add_library(boards.stm32f429i_disco_lcd ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(boards.stm32f429i_disco_lcd PUBLIC
    drivers.display.ili9341
    hal.interfaces
    services.peripheral
)

target_sources(boards.stm32f429i_disco_lcd PRIVATE
    Stm32f429iDiscoLcdSetup.cpp
    Stm32f429iDiscoLcdSetup.hpp
)

add_subdirectory(test)
