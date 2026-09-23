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
    add_subdirectory(compile_fail)
endif()
