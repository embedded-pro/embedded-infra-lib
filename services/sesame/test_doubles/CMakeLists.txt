add_library(services.sesame_test_doubles INTERFACE)
emil_build_for(services.sesame_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.sesame_test_doubles INTERFACE
    gmock
    services.sesame
)

target_sources(services.sesame_test_doubles PRIVATE
    SesameMock.hpp
)
