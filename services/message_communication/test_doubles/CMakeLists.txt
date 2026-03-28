add_library(services.message_communication_test_doubles INTERFACE)
emil_build_for(services.message_communication_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.message_communication_test_doubles INTERFACE
    gmock
    services.message_communication
)

target_sources(services.message_communication_test_doubles PRIVATE
    MessageCommunicationMock.hpp
)
