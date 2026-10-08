add_library(drivers.microphones.pdm_test_doubles INTERFACE)
emil_build_for(drivers.microphones.pdm_test_doubles BOOL BUILD_TESTING)

target_link_libraries(drivers.microphones.pdm_test_doubles INTERFACE
    drivers.microphones.pdm
    gmock
    hal.interfaces_test_doubles
)

target_sources(drivers.microphones.pdm_test_doubles PRIVATE
    PdmMicrophoneChipTest.hpp
    PdmToPcmMock.hpp
)
