if (EMIL_HOST_BUILD)
    add_library(services.echo_attributes ${EMIL_EXCLUDE_FROM_ALL} STATIC)

    set_target_properties(services.echo_attributes PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_link_libraries(services.echo_attributes PUBLIC
        protobuf::libprotoc
        protobuf::libprotobuf
    )

    target_compile_definitions(services.echo_attributes PRIVATE
        LIBPROTOC_EXPORTS
    )

    protocol_buffer_all(services.echo_attributes ${CMAKE_CURRENT_LIST_DIR}/EchoAttributes.proto)

    install(FILES ${CMAKE_CURRENT_LIST_DIR}/EchoAttributes.proto TYPE INCLUDE)
else()
    add_library(services.echo_attributes ${EMIL_EXCLUDE_FROM_ALL} INTERFACE)

    set_target_properties(services.echo_attributes PROPERTIES PROTOBUF_INCLUDES ${CMAKE_CURRENT_LIST_DIR})
endif()
