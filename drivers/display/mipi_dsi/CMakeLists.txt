add_library(drivers.display.mipi_dsi ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.display.mipi_dsi PUBLIC
    hal.interfaces
    infra.timer
    infra.util
)

target_sources(drivers.display.mipi_dsi PRIVATE
    MipiDcs.hpp
    MipiDsiDisplay.cpp
    MipiDsiDisplay.hpp
    MipiDsiPanelCore.cpp
    MipiDsiPanelCore.hpp
    MipiDsiVideoPanel.cpp
    MipiDsiVideoPanel.hpp
)

add_subdirectory(test)
