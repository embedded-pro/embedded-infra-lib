add_library(services.peripheral ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.peripheral PUBLIC
    hal.interfaces
    infra.event
    infra.timer
    infra.util
)

target_sources(services.peripheral PRIVATE
    DebouncedButton.cpp
    DebouncedButton.hpp
    DebugLed.cpp
    DebugLed.hpp
    GpioPinInverted.cpp
    GpioPinInverted.hpp
    I2cMultipleAccess.cpp
    I2cMultipleAccess.hpp
    LowPowerSerialCommunication.cpp
    LowPowerSerialCommunication.hpp
    LowPowerSpiMaster.cpp
    LowPowerSpiMaster.hpp
    RepeatingButton.cpp
    RepeatingButton.hpp
    SerialCommunicationLoopback.cpp
    SerialCommunicationLoopback.hpp
    SignalLed.cpp
    SignalLed.hpp
    SpiMasterWithChipSelect.cpp
    SpiMasterWithChipSelect.hpp
    SpiMultipleAccess.cpp
    SpiMultipleAccess.hpp
)

add_subdirectory(test)
