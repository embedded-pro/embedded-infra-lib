add_library(services.hil.commands ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.hil.commands PUBLIC
    services.hil
)

target_sources(services.hil.commands PRIVATE
    AdcCommands.cpp
    AdcCommands.hpp
    CanCommands.cpp
    CanCommands.hpp
    ComparatorCommands.cpp
    ComparatorCommands.hpp
    EepromCommands.cpp
    EepromCommands.hpp
    EthernetCommands.cpp
    EthernetCommands.hpp
    GpioCommands.cpp
    GpioCommands.hpp
    PwmCommands.cpp
    PwmCommands.hpp
    QeiCommands.cpp
    QeiCommands.hpp
    SingleInstance.cpp
    SingleInstance.hpp
    SpiCommands.cpp
    SpiCommands.hpp
    UartCommands.cpp
    UartCommands.hpp
    UnsupportedCommands.cpp
    UnsupportedCommands.hpp
    WatchDogCommands.cpp
    WatchDogCommands.hpp
)
