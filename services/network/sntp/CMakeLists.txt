add_library(services.network.sntp ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.sntp PUBLIC
    services.network.connection
)

target_sources(services.network.sntp PRIVATE
    SntpClient.cpp
    SntpClient.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
