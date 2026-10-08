add_library(drivers.microphones.pdm ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.microphones.pdm PUBLIC
    hal.interfaces
    infra.util
)

target_sources(drivers.microphones.pdm PRIVATE
    PdmMicrophone.cpp
    PdmMicrophone.hpp
    PdmToPcm.hpp
)

add_subdirectory(test_doubles)
add_subdirectory(test)
