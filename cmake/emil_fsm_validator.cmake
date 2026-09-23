function(emil_add_fsm_validator target)
    set(options STRICT)
    set(oneValueArgs MERMAID_DIR)
    set(multiValueArgs SOURCES LINK_LIBRARIES)
    cmake_parse_arguments(PARSE_ARGV 1 FSM_VALIDATOR "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if (NOT EMIL_HOST_BUILD)
        return()
    endif()

    add_executable(${target})
    target_sources(${target} PRIVATE ${FSM_VALIDATOR_SOURCES})
    target_link_libraries(${target} PRIVATE application.fsm_validator ${FSM_VALIDATOR_LINK_LIBRARIES})

    set(arguments "")
    if (FSM_VALIDATOR_STRICT)
        list(APPEND arguments --strict)
    endif()

    get_target_property(exclude ${target} EXCLUDE_FROM_ALL)
    if (NOT ${exclude})
        add_test(NAME ${target} COMMAND ${target} ${arguments})
    endif()

    if (FSM_VALIDATOR_MERMAID_DIR)
        add_custom_target(${target}.mermaid
            COMMAND ${CMAKE_COMMAND} -E make_directory ${FSM_VALIDATOR_MERMAID_DIR}
            COMMAND ${target} --mermaid ${FSM_VALIDATOR_MERMAID_DIR}
            DEPENDS ${target}
            COMMENT "Writing state machine diagrams of ${target} to ${FSM_VALIDATOR_MERMAID_DIR}"
        )
    endif()
endfunction()
