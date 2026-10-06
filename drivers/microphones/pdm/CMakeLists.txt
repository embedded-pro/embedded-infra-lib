add_library(drivers.microphones.pdm INTERFACE)

target_link_libraries(drivers.microphones.pdm INTERFACE
    infra.util
)

target_sources(drivers.microphones.pdm PRIVATE
    PdmToPcm.hpp
)

add_subdirectory(test_doubles)
