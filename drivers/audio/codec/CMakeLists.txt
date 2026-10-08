add_library(drivers.audio.codec ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.audio.codec PUBLIC
    hal.interfaces
    infra.event
    infra.util
)

target_sources(drivers.audio.codec PRIVATE
    CodecAudioOutput.cpp
    CodecAudioOutput.hpp
)

add_subdirectory(test)
