function(services_fsm_compile_fail name expected)
    add_library(services.fsm_compile_ok_${name} OBJECT ${name}.cpp Toggle.hpp)
    target_link_libraries(services.fsm_compile_ok_${name} PRIVATE services.fsm)

    add_library(services.fsm_compile_fail_${name} EXCLUDE_FROM_ALL OBJECT ${name}.cpp Toggle.hpp)
    target_link_libraries(services.fsm_compile_fail_${name} PRIVATE services.fsm)
    target_compile_definitions(services.fsm_compile_fail_${name} PRIVATE EMIL_FSM_COMPILE_FAIL)

    add_test(NAME services.fsm_compile_fail_${name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target services.fsm_compile_fail_${name} --config $<CONFIG>
    )
    set_tests_properties(services.fsm_compile_fail_${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endfunction()

services_fsm_compile_fail(UncheckedConstruction "protected")
services_fsm_compile_fail(DisallowedTransition "disallowedTransition: a row makes a transition")
services_fsm_compile_fail(WarningIsError "unusedEvent: no row handles an event")
