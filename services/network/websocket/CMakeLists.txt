add_library(services.network.websocket ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.websocket PUBLIC
    services.network.connection
    services.network.http
)

target_sources(services.network.websocket PRIVATE
    WebSocket.cpp
    WebSocket.hpp
)

if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    target_sources(services.network.websocket PRIVATE
        HttpPageWebSocket.cpp
        HttpPageWebSocket.hpp
        WebSocketClientConnectionObserver.cpp
        WebSocketClientConnectionObserver.hpp
        WebSocketServerConnectionObserver.cpp
        WebSocketServerConnectionObserver.hpp
    )
endif()

add_subdirectory(test)
