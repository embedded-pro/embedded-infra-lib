add_executable(services.echo_core_test)
emil_build_for(services.echo_core_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.echo_core_test)

protocol_buffer_echo_cpp(services.echo_core_test TestMessages.proto)

target_sources(services.echo_core_test PRIVATE
    TestProtoMessageReceiver.cpp
    TestProtoMessageSender.cpp
    TestServiceForwarder.cpp
)

target_link_libraries(services.echo_core_test PUBLIC
    gmock_main
    infra.event_test_helper
    infra.util_test_helper
    services.echo_core
    services.echo_core_test_doubles
)
