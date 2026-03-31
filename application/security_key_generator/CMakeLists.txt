add_executable(application.security_key_generator)
emil_build_for(application.security_key_generator HOST All PREREQUISITE_BOOL EMIL_STANDALONE)
emil_install(application.security_key_generator EXPORT emilApplicationTargets DESTINATION bin)

target_link_libraries(application.security_key_generator PUBLIC
    args
    application.material_generator
)

target_sources(application.security_key_generator PRIVATE
    Main.cpp
)

add_subdirectory(material_generator)