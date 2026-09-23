add_library(services.fsm_test_doubles INTERFACE)
emil_build_for(services.fsm_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.fsm_test_doubles INTERFACE
    gmock
    services.fsm
)

target_sources(services.fsm_test_doubles PRIVATE
    StateMachineObserverMock.hpp
    StateMachineTester.hpp
    UncheckedTableStateMachine.hpp
)
