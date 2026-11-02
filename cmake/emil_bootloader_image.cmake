# Post-build steps for images of the bootloader package (docs/Bootloader.md), run with application.bootloader_image
# and EMIL_OBJ_COPY_TOOL. A cross-compiling build gets the tool through emil_native_executables. Add them before
# emil_generate_artifacts, so the .bin and .hex files it writes carry what they wrote into the linked image.
#
# emil_bootloader_key_slot(<target> KEYS <key set> [SECTION <section>])
#     Writes the key slot made from KEYS into SECTION (default .bootloader_keys) of the linked bootloader.
#     A change to KEYS relinks the target.
#
# emil_bootloader_image_descriptor(<target> [SECTION <section>])
#     Writes the image descriptor into SECTION (default .image_descriptor) of the linked application: it covers the
#     image as a debugger programs it, every section but SECTION from the first to the last, with gaps erased.
#
# emil_bootloader_combined_hex(<name> OUTPUT <file> BOOTLOADER <target> APPLICATION <target>)
#     Adds target <name>, part of ALL, which joins the .hex files emil_generate_artifacts writes for BOOTLOADER and
#     APPLICATION into OUTPUT, one file a debugger programs with both.

function(emil_fetch_bootloader_image)
    emil_native_executables(bootloader_image
        TARGETS application.bootloader_image
        CMAKE_ARGS
            "-DEMIL_INCLUDE_UPGRADE=On"
            "-DEMIL_INCLUDE_ECHO=Off"
            "-DEMIL_BUILD_ECHO_COMPILERS=Off"
    )
endfunction()

function(emil_bootloader_require_object_copy function)
    if (NOT EMIL_OBJ_COPY_TOOL)
        message(FATAL_ERROR "${function} needs EMIL_OBJ_COPY_TOOL, which the toolchain file sets")
    endif()
endfunction()

function(emil_bootloader_key_slot target)
    cmake_parse_arguments(PARSE_ARGV 1 KEY_SLOT "" "KEYS;SECTION" "")

    if (NOT KEY_SLOT_KEYS)
        message(FATAL_ERROR "emil_bootloader_key_slot: KEYS names no key set")
    endif()
    if (NOT KEY_SLOT_SECTION)
        set(KEY_SLOT_SECTION .bootloader_keys)
    endif()

    emil_bootloader_require_object_copy(emil_bootloader_key_slot)
    emil_fetch_bootloader_image()

    set(key_slot "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.key_slot.bin")
    add_dependencies(${target} application.bootloader_image)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${KEY_SLOT_KEYS}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND application.bootloader_image bootloader-keys --keys "${KEY_SLOT_KEYS}" --output "${key_slot}"
        COMMAND "${EMIL_OBJ_COPY_TOOL}" --update-section "${KEY_SLOT_SECTION}=${key_slot}" "$<TARGET_FILE:${target}>"
        VERBATIM
    )
endfunction()

function(emil_bootloader_image_descriptor target)
    cmake_parse_arguments(PARSE_ARGV 1 DESCRIPTOR "" "SECTION" "")

    if (NOT DESCRIPTOR_SECTION)
        set(DESCRIPTOR_SECTION .image_descriptor)
    endif()

    emil_bootloader_require_object_copy(emil_bootloader_image_descriptor)
    emil_fetch_bootloader_image()

    set(image "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.image.bin")
    set(descriptor "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.descriptor.bin")
    add_dependencies(${target} application.bootloader_image)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${EMIL_OBJ_COPY_TOOL}" -O binary "--remove-section=${DESCRIPTOR_SECTION}" --gap-fill 0xff "$<TARGET_FILE:${target}>" "${image}"
        COMMAND application.bootloader_image descriptor --image "${image}" --output "${descriptor}"
        COMMAND "${EMIL_OBJ_COPY_TOOL}" --update-section "${DESCRIPTOR_SECTION}=${descriptor}" "$<TARGET_FILE:${target}>"
        VERBATIM
    )
endfunction()

function(emil_bootloader_combined_hex name)
    cmake_parse_arguments(PARSE_ARGV 1 COMBINED "" "OUTPUT;BOOTLOADER;APPLICATION" "")

    foreach(argument IN ITEMS OUTPUT BOOTLOADER APPLICATION)
        if (NOT COMBINED_${argument})
            message(FATAL_ERROR "emil_bootloader_combined_hex: ${argument} is missing")
        endif()
    endforeach()

    emil_fetch_bootloader_image()

    set(bootloader_hex "$<TARGET_FILE_DIR:${COMBINED_BOOTLOADER}>/$<TARGET_FILE_BASE_NAME:${COMBINED_BOOTLOADER}>.hex")
    set(application_hex "$<TARGET_FILE_DIR:${COMBINED_APPLICATION}>/$<TARGET_FILE_BASE_NAME:${COMBINED_APPLICATION}>.hex")
    add_custom_command(OUTPUT "${COMBINED_OUTPUT}"
        COMMAND application.bootloader_image combine --output "${COMBINED_OUTPUT}" "${bootloader_hex}" "${application_hex}"
        DEPENDS ${COMBINED_BOOTLOADER} ${COMBINED_APPLICATION} application.bootloader_image
        VERBATIM
    )
    add_custom_target(${name} ALL DEPENDS "${COMBINED_OUTPUT}")
endfunction()
