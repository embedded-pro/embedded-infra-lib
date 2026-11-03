add_library(application.bootloader_image_lib ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(application.bootloader_image_lib PUBLIC
    bootloader
    crypto.micro_ecc
    upgrade.pack_builder
)

target_sources(application.bootloader_image_lib PRIVATE
    Command.cpp
    Command.hpp
    ElfSection.cpp
    ElfSection.hpp
    Image.cpp
    Image.hpp
    KeySet.cpp
    KeySet.hpp
    Pack.cpp
    Pack.hpp
)

add_executable(application.bootloader_image)
emil_build_for(application.bootloader_image HOST All PREREQUISITE_BOOL EMIL_STANDALONE)
emil_install(application.bootloader_image EXPORT emilApplicationTargets DESTINATION bin)

target_link_libraries(application.bootloader_image PRIVATE
    application.bootloader_image_lib
)

target_sources(application.bootloader_image PRIVATE
    Main.cpp
)

add_subdirectory(test)
