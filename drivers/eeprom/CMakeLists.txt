add_library(drivers.eeprom ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(drivers.eeprom PUBLIC
    hal.interfaces
    infra.timer
    infra.util
)

target_sources(drivers.eeprom PRIVATE
    Eeprom24xx.cpp
    Eeprom24xx.hpp
)

add_subdirectory(test)
