add_library(services.terminal ${EMIL_EXCLUDE_FROM_ALL})
emil_build_for(services.terminal HOST All)

target_include_directories(services.terminal PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(services.terminal PRIVATE
    TerminalTypes.hpp
    TerminalScreen.hpp
    TerminalScreen.cpp
    Vt100Parser.hpp
    Vt100Parser.cpp
    Vt100Terminal.hpp
    Vt100Terminal.cpp
)

add_subdirectory(test)
