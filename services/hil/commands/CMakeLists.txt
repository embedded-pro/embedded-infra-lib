add_library(services.hil.commands ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.hil.commands PUBLIC
    services.hil
)

target_sources(services.hil.commands PRIVATE
    HilAdcCommands.cpp
    HilAdcCommands.hpp
    HilCanCommands.cpp
    HilCanCommands.hpp
    HilComparatorCommands.cpp
    HilComparatorCommands.hpp
    HilEepromCommands.cpp
    HilEepromCommands.hpp
    HilEthernetCommands.cpp
    HilEthernetCommands.hpp
    HilGpioCommands.cpp
    HilGpioCommands.hpp
    HilPwmCommands.cpp
    HilPwmCommands.hpp
    HilQeiCommands.cpp
    HilQeiCommands.hpp
    HilSingleInstance.cpp
    HilSingleInstance.hpp
    HilSpiCommands.cpp
    HilSpiCommands.hpp
    HilUartCommands.cpp
    HilUartCommands.hpp
    HilUnsupportedCommands.cpp
    HilUnsupportedCommands.hpp
    HilWatchDogCommands.cpp
    HilWatchDogCommands.hpp
)
