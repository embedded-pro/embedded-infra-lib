if (EMIL_HOST_BUILD AND EMIL_INCLUDE_MBEDTLS)
    add_executable(application.echo_console ${EMIL_EXCLUDE_FROM_ALL})
    emil_install(application.echo_console DESTINATION bin)

    target_compile_options(application.echo_console PRIVATE
        # Clang complains about "captured structured bindings are a C++20 extension", which was an oversight in the C++ standard.
        # Rather than setting the language to C++20, this is a small thing that can simply be ignored
        $<$<CXX_COMPILER_ID:Clang>:-Wno-c++20-extensions>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wno-c++20-extensions>
    )

    target_link_libraries(application.echo_console PUBLIC
        args
        hal.generic
        application.protoc_echo_plugin_lib
        services.echo_core
        services.echo
        services.network_instantiations
        services.sesame
    )

    target_sources(application.echo_console PRIVATE
        Console.cpp
        Console.hpp
        Main.cpp
    )
endif()
