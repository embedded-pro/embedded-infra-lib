add_library(bootloader ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(bootloader PUBLIC
    hal.synchronous_interfaces
    infra.util
    services.crypto
    upgrade.boot_loader
    upgrade.pack
)

target_sources(bootloader PRIVATE
    ApplicationCheck.cpp
    ApplicationCheck.hpp
    ApplicationInstaller.cpp
    ApplicationInstaller.hpp
    BootSequence.cpp
    BootSequence.hpp
    ImageFormat.hpp
    JournalPackStatus.cpp
    JournalPackStatus.hpp
    StatusJournal.cpp
    StatusJournal.hpp
)

add_subdirectory(test)
