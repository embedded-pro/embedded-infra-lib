add_library(services.fsm ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.fsm PUBLIC
    infra.event
    infra.timer
    infra.util
    services.tracer
)

target_sources(services.fsm PRIVATE
    AlternativeId.hpp
    StateMachine.hpp
    StateMachineMermaid.hpp
    StateMachineTracer.hpp
    StateTimeouts.hpp
    TableStateMachine.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
