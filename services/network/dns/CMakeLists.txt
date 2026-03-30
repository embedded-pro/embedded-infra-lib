add_library(services.network.dns ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.dns PUBLIC
    services.network.connection
)

target_sources(services.network.dns PRIVATE
    BonjourServer.cpp
    BonjourServer.hpp
    Dns.cpp
    Dns.hpp
    DnsResolver.cpp
    DnsResolver.hpp
    DnsServer.cpp
    DnsServer.hpp
    LlmnrResponder.cpp
    LlmnrResponder.hpp
    MdnsClient.cpp
    MdnsClient.hpp
    NameResolverCache.cpp
    NameResolverCache.hpp
)

add_subdirectory(test)
