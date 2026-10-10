add_library(boards.rk043fn48h ${EMIL_EXCLUDE_FROM_ALL} INTERFACE)

target_link_libraries(boards.rk043fn48h INTERFACE
    hal.interfaces
)

target_sources(boards.rk043fn48h PRIVATE
    Rk043fn48h.hpp
)

add_subdirectory(test)
