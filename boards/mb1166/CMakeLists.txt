add_library(boards.mb1166 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(boards.mb1166 PUBLIC
    drivers.display.mipi_dsi
    hal.interfaces
)

target_sources(boards.mb1166 PRIVATE
    Mb1166Setup.cpp
    Mb1166Setup.hpp
)

add_subdirectory(test)
