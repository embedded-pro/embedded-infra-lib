add_library(boards.stm32f4_disco_audio ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(boards.stm32f4_disco_audio PUBLIC
    drivers.audio.cs43l22
    hal.interfaces
)

target_sources(boards.stm32f4_disco_audio PRIVATE
    Stm32f4DiscoAudioSetup.cpp
    Stm32f4DiscoAudioSetup.hpp
)

add_subdirectory(test)
