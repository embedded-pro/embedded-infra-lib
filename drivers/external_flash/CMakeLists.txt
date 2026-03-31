add_library(drivers.external_flash ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.external_flash PUBLIC
    services.flash
    hal.interfaces
)

target_sources(drivers.external_flash PRIVATE
    FlashQuadSpiCypressFll.cpp
    FlashQuadSpiCypressFll.hpp
    FlashQuadSpiMicronN25q.cpp
    FlashQuadSpiMicronN25q.hpp
)

add_subdirectory(test)
