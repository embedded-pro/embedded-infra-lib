add_executable(services.fsm_test)
emil_build_for(services.fsm_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.fsm_test)

target_link_libraries(services.fsm_test PUBLIC
    gmock_main
    services.fsm
    services.fsm_test_doubles
    infra.event_test_helper
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.fsm_test PRIVATE
    JobLifecycle.hpp
    TestAlternativeId.cpp
    TestStateMachineIntegration.cpp
    TestStateMachineMermaid.cpp
    TestStateMachineTracer.cpp
    TestStateMachineValidation.cpp
    TestStateTimeouts.cpp
    TestTableStateMachine.cpp
    TestTransitionTableAnalysis.cpp
)

if (EMIL_BUILD_TESTS)
    function(services_fsm_compile_fail name expected)
        add_library(services.fsm_compile_ok_${name} OBJECT CompileFail${name}.cpp CompileFailToggle.hpp)
        target_link_libraries(services.fsm_compile_ok_${name} PRIVATE services.fsm)

        add_library(services.fsm_compile_fail_${name} EXCLUDE_FROM_ALL OBJECT CompileFail${name}.cpp CompileFailToggle.hpp)
        target_link_libraries(services.fsm_compile_fail_${name} PRIVATE services.fsm)
        target_compile_definitions(services.fsm_compile_fail_${name} PRIVATE EMIL_FSM_COMPILE_FAIL)

        add_test(NAME services.fsm_compile_fail_${name}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target services.fsm_compile_fail_${name} --config $<CONFIG>
        )
        set_tests_properties(services.fsm_compile_fail_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")

        emil_exclude_from_coverage(services.fsm_compile_ok_${name})
        emil_exclude_from_coverage(services.fsm_compile_fail_${name})
    endfunction()

    services_fsm_compile_fail(UncheckedConstruction "protected")
    services_fsm_compile_fail(DisallowedTransition "disallowedTransition: a row makes a transition")
    services_fsm_compile_fail(WarningIsError "unusedEvent: no row handles an event")
endif()
