add_library(emil.services.terminal ${EMIL_EXCLUDE_FROM_ALL})
emil_build_for(emil.services.terminal HOST All)

target_include_directories(emil.services.terminal PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(emil.services.terminal PRIVATE
    TerminalTypes.hpp
    TerminalScreen.hpp
    TerminalScreen.cpp
    Vt100Parser.hpp
    Vt100Parser.cpp
    Vt100Terminal.hpp
    Vt100Terminal.cpp
)

add_subdirectory(test)
