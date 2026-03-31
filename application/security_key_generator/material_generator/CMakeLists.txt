add_library(application.material_generator STATIC EXCLUDE_FROM_ALL)

target_link_libraries(application.material_generator PUBLIC
    hal.generic
    upgrade.pack_keys
    upgrade.pack_builder
)

target_sources(application.material_generator PRIVATE
    MaterialGenerator.cpp
    MaterialGenerator.hpp
)

add_subdirectory(test)
