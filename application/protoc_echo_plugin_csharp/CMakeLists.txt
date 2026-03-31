if (EMIL_HOST_BUILD AND NOT CMAKE_CROSSCOMPILING)
    add_executable(application.protoc_echo_plugin_csharp)
    emil_build_for(application.protoc_echo_plugin_csharp BOOL EMIL_BUILD_ECHO_COMPILERS)
    emil_install(application.protoc_echo_plugin_csharp EXPORT emilApplicationTargets DESTINATION bin)

    target_link_libraries(application.protoc_echo_plugin_csharp PUBLIC
        application.protoc_echo_plugin_lib
    )

    target_sources(application.protoc_echo_plugin_csharp PRIVATE
        Main.cpp
        ProtoCEchoPluginCSharp.cpp
        ProtoCEchoPluginCSharp.hpp
    )
endif()
