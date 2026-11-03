add_executable(bootloader_test)
emil_build_for(bootloader_test BOOL EMIL_BUILD_TESTS)
emil_add_test(bootloader_test)

target_link_libraries(bootloader_test PUBLIC
    gmock_main
    application.bootloader_image_lib
    bootloader
)

target_sources(bootloader_test PRIVATE
    FlashFake.hpp
    PackBuilder.hpp
    TestApplicationCheck.cpp
    TestBootSequence.cpp
    TestImages.hpp
    TestStatusJournal.cpp
)
