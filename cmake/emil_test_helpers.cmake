option(EMIL_ENABLE_COVERAGE "Enable compiler flags for code coverage measurements" Off)

function(emil_fetch_googletest)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG        v1.14.0
    )

    set(gtest_force_shared_crt On CACHE BOOL "" FORCE) # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(INSTALL_GTEST Off CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES FOLDER External/GoogleTest)
    set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES EXCLUDE_FROM_COVERAGE TRUE)
    mark_as_advanced(BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS gmock_build_tests gtest_build_samples test_build_tests gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols)
endfunction()

function(emil_enable_testing)
    include(GoogleTest)

    emil_fetch_googletest()
endfunction()

function(emil_add_test target)
    get_target_property(exclude ${target} EXCLUDE_FROM_ALL)
    if (NOT ${exclude})
        add_test(NAME ${target} COMMAND ${target})
        emil_exclude_from_coverage(${target})
    endif()
endfunction()
