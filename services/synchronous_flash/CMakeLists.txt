add_library(services.synchronous_flash ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.synchronous_flash PUBLIC
    hal.interfaces
    hal.synchronous_interfaces
)

target_sources(services.synchronous_flash PRIVATE
    SynchronousFlashQuadSpi.cpp
    SynchronousFlashQuadSpi.hpp
    SynchronousFlashRegion.cpp
    SynchronousFlashRegion.hpp
    SynchronousFlashSpi.cpp
    SynchronousFlashSpi.hpp
)

add_subdirectory(test)
