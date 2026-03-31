if (EMIL_FETCH_ECHO_COMPILERS)
    set(NOT_EMIL_FETCH_ECHO_COMPILERS false)
else()
    set(NOT_EMIL_FETCH_ECHO_COMPILERS true)
endif()

add_executable(application.protoc_echo_plugin_test)
emil_build_for(application.protoc_echo_plugin_test BOOL EMIL_BUILD_TESTS PREREQUISITE_BOOL NOT_EMIL_FETCH_ECHO_COMPILERS)
emil_add_test(application.protoc_echo_plugin_test)

protocol_buffer_echo_cpp(application.protoc_echo_plugin_test ../../../services/echo_core/test/TestMessages.proto)

target_sources(application.protoc_echo_plugin_test PRIVATE
    TestCppFormatter.cpp
    TestProtoCEchoPlugin.cpp
)

target_link_libraries(application.protoc_echo_plugin_test PUBLIC
    gmock_main
    infra.event_test_helper
    infra.util_test_helper
    application.protoc_echo_plugin_lib
    services.network.connection_test_doubles
)
