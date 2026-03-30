add_library(services.synchronous_peripheral ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.synchronous_peripheral PUBLIC
    hal.interfaces
    hal.synchronous_interfaces
)

target_sources(services.synchronous_peripheral PRIVATE
    SynchronousSpiMasterWithChipSelect.cpp
    SynchronousSpiMasterWithChipSelect.hpp
)
