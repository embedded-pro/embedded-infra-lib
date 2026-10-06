add_library(boards.boostxl_k350qvg_s1 ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(boards.boostxl_k350qvg_s1 PUBLIC
    drivers.display.ssd2119
    hal.interfaces
)

target_sources(boards.boostxl_k350qvg_s1 PRIVATE
    BoostxlK350qvgS1Setup.cpp
    BoostxlK350qvgS1Setup.hpp
)

add_subdirectory(test)
