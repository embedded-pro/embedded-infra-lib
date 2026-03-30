add_library(services.crypto_test_doubles INTERFACE)
emil_build_for(services.crypto_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.crypto_test_doubles INTERFACE
    gmock
    services.crypto
)

target_sources(services.crypto_test_doubles PRIVATE
    EllipticCurveMock.hpp
)
