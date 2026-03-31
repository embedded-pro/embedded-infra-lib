if (EMIL_HOST_BUILD AND NOT EMIL_FETCH_ECHO_COMPILERS)
    add_library(application.protoc_echo_plugin_lib ${EMIL_EXCLUDE_FROM_ALL} STATIC)

    target_compile_definitions(application.protoc_echo_plugin_lib PRIVATE
        LIBPROTOC_EXPORTS
    )

    target_link_libraries(application.protoc_echo_plugin_lib PUBLIC
        services.echo_attributes
        infra.syntax
    )

    target_include_directories(application.protoc_echo_plugin_lib PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_sources(application.protoc_echo_plugin_lib PRIVATE
        CppFormatter.cpp
        CppFormatter.hpp
        EchoObjects.cpp
        EchoObjects.hpp
        ProtoCEchoPlugin.cpp
        ProtoCEchoPlugin.hpp
    )

    if (NOT CMAKE_CROSSCOMPILING)
        add_executable(application.protoc_echo_plugin)
        emil_build_for(application.protoc_echo_plugin BOOL EMIL_BUILD_ECHO_COMPILERS)
        emil_install(application.protoc_echo_plugin EXPORT emilApplicationTargets DESTINATION bin)

        target_link_libraries(application.protoc_echo_plugin PUBLIC
            application.protoc_echo_plugin_lib
        )

        target_sources(application.protoc_echo_plugin PRIVATE
            Main.cpp
        )
    endif()
endif()

add_subdirectory(test)
