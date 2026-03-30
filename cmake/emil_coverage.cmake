define_property(TARGET PROPERTY EXCLUDE_FROM_COVERAGE)

function(emil_exclude_from_coverage target)
    set_target_properties(${target} PROPERTIES EXCLUDE_FROM_COVERAGE TRUE)
endfunction()

function(emil_exclude_directory_from_coverage directory)
    emil_get_subdirectories(directories DIRECTORIES ${directory})
    list(APPEND directories ${directory})
    emil_get_targets_from_directories(targets DIRECTORIES ${directories})

    foreach(target ${targets})
       emil_exclude_from_coverage(${target})
    endforeach()
endfunction()

function(emil_enable_coverage_for target)
    if (EMIL_ENABLE_COVERAGE)
        message(DEBUG "enable coverage for: ${target}")
        target_compile_options(${target} PRIVATE
            -g -O0 --coverage -fprofile-arcs -ftest-coverage -fno-inline
            $<$<COMPILE_LANGUAGE:CXX>:-fno-elide-constructors>
        )

        if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_link_libraries(${target} PRIVATE gcov)
        else()
            target_link_options(${target} PRIVATE --coverage)
        endif()
    endif()
endfunction()

function(emil_coverage_targets directories)
    set(singleArgs COVERAGE_FILE)
    set(multiValueArgs DIRECTORIES)
    cmake_parse_arguments(PARSE_ARGV 0 MY "" "${singleArgs}" "${multiValueArgs}")

    emil_get_subdirectories(subdirectories DIRECTORIES ${MY_DIRECTORIES})
    emil_get_targets_from_directories(targets DIRECTORIES ${subdirectories})

    set(non_generated_target_sources)
    foreach(target ${targets})
        get_target_property(exclude ${target} EXCLUDE_FROM_COVERAGE)
        get_target_property(type ${target} TYPE)

        if (NOT exclude AND NOT ${type} STREQUAL "INTERFACE_LIBRARY" AND NOT ${type} STREQUAL "UTILITY")
            emil_enable_coverage_for(${target})
        else()
            message(DEBUG "skipping coverage for: ${target}")
        endif()
    endforeach()
endfunction()
