add_executable(application.bootloader_image_test)
emil_build_for(application.bootloader_image_test BOOL EMIL_BUILD_TESTS)
emil_add_test(application.bootloader_image_test)

target_link_libraries(application.bootloader_image_test PUBLIC
    gmock_main
    application.bootloader_image_lib
)

target_sources(application.bootloader_image_test PRIVATE
    ElfFile.hpp
    HexRecord.hpp
    TestCommand.cpp
    TestElfSection.cpp
    TestImage.cpp
    TestKeySet.cpp
    TestPack.cpp
)
