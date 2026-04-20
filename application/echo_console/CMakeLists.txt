if (EMIL_HOST_BUILD AND EMIL_INCLUDE_MBEDTLS)
    add_executable(application.echo_console ${EMIL_EXCLUDE_FROM_ALL})
    emil_install(application.echo_console DESTINATION bin)

    target_link_libraries(application.echo_console PUBLIC
        args
        hal.generic
        application.protoc_echo_plugin_lib
        services.echo_core
        services.echo
        services.network_instantiations
        services.sesame
        services.network.echo
    )

    target_sources(application.echo_console PRIVATE
        Console.cpp
        Console.hpp
        Main.cpp
    )
endif()
