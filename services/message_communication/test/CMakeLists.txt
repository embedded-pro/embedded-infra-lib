add_executable(services.message_communication_test)
emil_build_for(services.message_communication_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.message_communication_test)

target_link_libraries(services.message_communication_test PUBLIC
    gmock_main
    services.message_communication
    services.message_communication_test_doubles
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(services.message_communication_test PRIVATE
    TestMessageCommunicationCobs.cpp
    TestMessageCommunicationWindowed.cpp
)
