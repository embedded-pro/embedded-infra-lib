
if (EMIL_INCLUDE_MBEDTLS)
    add_executable(application.material_generator_test)
    emil_build_for(application.material_generator_test BOOL EMIL_BUILD_TESTS)
    emil_add_test(application.material_generator_test)

    target_link_libraries(application.material_generator_test PUBLIC
        gmock_main
        application.material_generator
        hal.interfaces_test_doubles
        infra.syntax
    )

    target_sources(application.material_generator_test PRIVATE
        $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestMaterialGenerator.cpp>
    )
endif()
