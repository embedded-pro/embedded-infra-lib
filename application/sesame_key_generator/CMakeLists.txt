if (EMIL_HOST_BUILD AND EMIL_INCLUDE_MBEDTLS)
    add_executable(application.sesame_key_generator ${EMIL_EXCLUDE_FROM_ALL})
    emil_install(application.sesame_key_generator DESTINATION bin)

    target_link_libraries(application.sesame_key_generator PUBLIC
        args
        hal.generic
        services.echo
        services.sesame
    )

    target_sources(application.sesame_key_generator PRIVATE
        Main.cpp
    )
endif()
