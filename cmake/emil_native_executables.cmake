# 'emil_native_executables' makes EmIL executables that run on the build host available to a build that does not have
# them, typically one that cross-compiles: it configures EmIL a second time with the host's own compiler, in a
# directory of its own, builds the executables there and imports them under their own names.
#
# emil_native_executables(<name> TARGETS <target>... [CMAKE_ARGS <argument>...])
#
# <name> names the native build: its directory is ${CMAKE_BINARY_DIR}/_native_<name>, its ExternalProject target
# emil_native_<name>. TARGETS that already exist in this build are used as they are; when all of them exist, nothing
# is added. CMAKE_ARGS configure the native build, after the defaults below, so they can override them.
# Sources this build already fetched for EmIL's dependencies are reused instead of being fetched again.

function(emil_native_executables name)
    cmake_parse_arguments(PARSE_ARGV 1 NATIVE "" "" "TARGETS;CMAKE_ARGS")

    set(missing "")
    foreach(target IN LISTS NATIVE_TARGETS)
        if (NOT TARGET ${target})
            list(APPEND missing ${target})
        endif()
    endforeach()

    if (NOT missing)
        return()
    endif()

    include(ExternalProject)
    include(FetchContent)

    set(binary_dir "${CMAKE_BINARY_DIR}/_native_${name}")
    set(output_dir "${binary_dir}/bin")
    get_filename_component(emil_source_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)

    if (CMAKE_GENERATOR MATCHES "^Ninja")
        set(generator "Ninja")
    else()
        set(generator "${CMAKE_GENERATOR}")
    endif()

    set(fetched_sources "")
    foreach(dependency IN ITEMS absl args googletest lwip mbedtls microecc protobuf protoc pugixml)
        FetchContent_GetProperties(${dependency})
        if (${dependency}_POPULATED)
            string(TOUPPER ${dependency} dependency_upper)
            list(APPEND fetched_sources "-DFETCHCONTENT_SOURCE_DIR_${dependency_upper}=${${dependency}_SOURCE_DIR}")
        endif()
    endforeach()

    set(byproducts "")
    foreach(target IN LISTS missing)
        list(APPEND byproducts "${output_dir}/${target}${CMAKE_HOST_EXECUTABLE_SUFFIX}")
    endforeach()

    ExternalProject_Add(emil_native_${name}
        SOURCE_DIR "${emil_source_dir}"
        BINARY_DIR "${binary_dir}"
        CMAKE_GENERATOR "${generator}"
        CMAKE_ARGS
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_TOOLCHAIN_FILE="
            "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${output_dir}"
            "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${output_dir}"
            "-DEMIL_BUILD_TESTS=Off"
            "-DEMIL_BUILD_EXAMPLES=Off"
            "-DEMIL_INCLUDE_LWIP=Off"
            "-DEMIL_INCLUDE_MBEDTLS=Off"
            ${fetched_sources}
            ${NATIVE_CMAKE_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target ${missing}
        BUILD_BYPRODUCTS ${byproducts}
        INSTALL_COMMAND ""
        BUILD_ALWAYS On
    )

    foreach(target IN LISTS missing)
        add_executable(${target} IMPORTED GLOBAL)
        set_target_properties(${target} PROPERTIES IMPORTED_LOCATION "${output_dir}/${target}${CMAKE_HOST_EXECUTABLE_SUFFIX}")
        add_dependencies(${target} emil_native_${name})
    endforeach()
endfunction()
