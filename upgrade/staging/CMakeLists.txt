add_library(upgrade.staging ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(upgrade.staging PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(upgrade.staging PUBLIC
    hal.interfaces
    infra.util
    upgrade.pack
)

target_sources(upgrade.staging PRIVATE
    PackStagingWriter.cpp
    PackStagingWriter.hpp
    StagedPackReader.cpp
    StagedPackReader.hpp
    UpgradePackCheck.cpp
    UpgradePackCheck.hpp
)

add_subdirectory(test)
