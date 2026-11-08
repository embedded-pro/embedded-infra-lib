add_executable(upgrade.staging_test)
emil_build_for(upgrade.staging_test BOOL EMIL_BUILD_TESTS)
emil_add_test(upgrade.staging_test)

target_link_libraries(upgrade.staging_test PUBLIC
    gmock_main
    upgrade.staging
    hal.interfaces_test_doubles
    infra.event_test_helper
)

target_sources(upgrade.staging_test PRIVATE
    TestPackStagingWriter.cpp
    TestStagedPackReader.cpp
    TestUpgradePackCheck.cpp
)
