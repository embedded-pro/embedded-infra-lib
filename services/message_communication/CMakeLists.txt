add_library(services.message_communication ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.message_communication PUBLIC
    hal.interfaces
    infra.event
    infra.stream
    infra.util
)

target_sources(services.message_communication PRIVATE
    MessageCommunication.hpp
    MessageCommunicationCobs.cpp
    MessageCommunicationCobs.hpp
    MessageCommunicationWindowed.cpp
    MessageCommunicationWindowed.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
