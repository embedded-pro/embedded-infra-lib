if (EMIL_HOST_BUILD AND NOT CMAKE_CROSSCOMPILING)
    add_executable(application.protoc_echo_plugin_java)
    emil_build_for(application.protoc_echo_plugin_java BOOL EMIL_BUILD_ECHO_COMPILERS)
    emil_install(application.protoc_echo_plugin_java EXPORT emilApplicationTargets DESTINATION bin)

    target_link_libraries(application.protoc_echo_plugin_java PUBLIC
        application.protoc_echo_plugin_lib
    )

    target_sources(application.protoc_echo_plugin_java PRIVATE
        Main.cpp
        ProtoCEchoPluginJava.cpp
        ProtoCEchoPluginJava.hpp
    )
endif()
