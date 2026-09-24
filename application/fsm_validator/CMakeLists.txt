add_library(application.fsm_validator_lib ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(application.fsm_validator_lib PUBLIC
    infra.stream
    services.fsm
)

target_sources(application.fsm_validator_lib PRIVATE
    FsmRegistry.cpp
    FsmRegistry.hpp
    FsmValidator.cpp
    FsmValidator.hpp
)

add_library(application.fsm_validator ${EMIL_EXCLUDE_FROM_ALL} OBJECT)

target_link_libraries(application.fsm_validator PUBLIC
    args
    application.fsm_validator_lib
)

target_sources(application.fsm_validator PRIVATE
    Main.cpp
)

add_subdirectory(test)
