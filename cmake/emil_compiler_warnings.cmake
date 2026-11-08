# Enables -Wall -Wextra for targets created after the call in the calling directory and below,
# minus the diagnostics that only produce false positives on EmIL and GoogleTest code.
#
# Kept in CMake rather than CMakePresets.json: a preset cannot branch on CMAKE_CXX_COMPILER_ID,
# and passing a GCC-only -Wno-* option to Clang raises -Wunknown-warning-option, which
# CMAKE_COMPILE_WARNING_AS_ERROR turns into a failed build.
function(emil_enable_warnings)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra)
    endif()

    # GCC inlines infra::Function::operator() into the caller and reports -Wmaybe-uninitialized
    # against it, naming an index into a single object. Definite -Wuninitialized stays on.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-maybe-uninitialized)
    endif()

    # Clang targeting MSVC enables a much broader diagnostic set than -Wall -Wextra ask for.
    # Every GTEST_TEST_F expansion then reports pre-C++20 compatibility warnings against C++20
    # code, at a volume that risks CI log-size limits; none of them bear on correctness.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat
                            -Wno-pre-c++14-compat -Wno-pre-c++17-compat -Wno-pre-c++20-compat
                            -Wno-global-constructors -Wno-exit-time-destructors -Wno-weak-vtables
                            -Wno-padded)
    endif()
endfunction()
