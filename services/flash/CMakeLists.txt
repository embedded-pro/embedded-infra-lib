add_library(services.flash ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.flash PUBLIC
    hal.interfaces
    infra.event
    infra.timer
    infra.util
)

target_sources(services.flash PRIVATE
    FlashAlign.cpp
    FlashAlign.hpp
    FlashDelegate.hpp
    FlashMultipleAccess.cpp
    FlashMultipleAccess.hpp
    FlashQuadSpi.cpp
    FlashQuadSpi.hpp
    FlashQuadSpiSingleSpeed.cpp
    FlashQuadSpiSingleSpeed.hpp
    FlashRegion.cpp
    FlashRegion.hpp
    FlashSpi.cpp
    FlashSpi.hpp
)

add_subdirectory(test)
