add_library(services.fsm ${EMIL_EXCLUDE_FROM_ALL} INTERFACE)

target_link_libraries(services.fsm INTERFACE
    infra.event
    infra.timer
    infra.util
    services.tracer
)

target_sources(services.fsm PRIVATE
    AlternativeId.hpp
    StateMachine.hpp
    StateMachineDefinition.hpp
    StateMachineMermaid.hpp
    StateMachineValidation.hpp
    StateMachineTracer.hpp
    StateTimeouts.hpp
    TableStateMachine.hpp
    TransitionRules.hpp
    TransitionTableAnalysis.hpp
    ValidatedTable.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
