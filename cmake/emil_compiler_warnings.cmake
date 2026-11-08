function(emil_enable_warnings)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra)
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-maybe-uninitialized)
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat
                            -Wno-pre-c++14-compat -Wno-pre-c++17-compat -Wno-pre-c++20-compat
                            -Wno-global-constructors -Wno-exit-time-destructors -Wno-weak-vtables
                            -Wno-padded)
    endif()
endfunction()
