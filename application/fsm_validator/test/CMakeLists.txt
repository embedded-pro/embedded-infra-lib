add_executable(application.fsm_validator_test)
emil_build_for(application.fsm_validator_test BOOL EMIL_BUILD_TESTS)
emil_add_test(application.fsm_validator_test)

target_link_libraries(application.fsm_validator_test PUBLIC
    gmock_main
    application.fsm_validator_lib
)

target_sources(application.fsm_validator_test PRIVATE
    TestFsmValidator.cpp
)

if (EMIL_BUILD_TESTS)
    emil_add_fsm_validator(application.fsm_validator_example
        SOURCES ExampleRegistration.cpp
        LINK_LIBRARIES services.fsm
        STRICT
        MERMAID_DIR ${CMAKE_CURRENT_BINARY_DIR}/mermaid
    )
endif()
