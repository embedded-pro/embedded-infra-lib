add_library(services.network.tls_test_doubles STATIC)
emil_build_for(services.network.tls_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.network.tls_test_doubles PUBLIC
    gmock
    services.network.tls
)

target_sources(services.network.tls_test_doubles PRIVATE
    Certificates.cpp
    Certificates.hpp
)
