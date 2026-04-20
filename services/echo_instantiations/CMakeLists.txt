add_library(services.echo_instantiations ${EMIL_EXCLUDE_FROM_ALL} STATIC)
emil_build_for(services.echo_instantiations HOST All)

target_sources(services.echo_instantiations PRIVATE
    EchoInstantiationOnNetwork.cpp
    EchoInstantiationOnNetwork.hpp
)

target_link_libraries(services.echo_instantiations PUBLIC
    services.network.echo
    services.tracer
)

target_link_libraries(services.echo_instantiations PRIVATE
    hal.generic
)
