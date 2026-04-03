add_subdirectory(args)
add_subdirectory(crypto)
if (EMIL_INCLUDE_ECHO)
    add_subdirectory(protobuf)
    add_subdirectory(protoc)
endif()

if (EMIL_INCLUDE_SEGGER_RTT)
    add_subdirectory(segger_rtt)
endif()

if (EMIL_INCLUDE_LWIP)
    add_subdirectory(lwip)
endif()
add_subdirectory(osal)

emil_exclude_directory_from_clang_format(.)
emil_exclude_directory_from_coverage(.)
