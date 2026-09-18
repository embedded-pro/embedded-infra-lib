add_executable(services.ble_test)
emil_build_for(services.ble_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.ble_test)

target_link_libraries(services.ble_test PUBLIC
    gmock_main
    infra.timer_test_helper
    services.ble
    services.ble_test_doubles
)

target_sources(services.ble_test PRIVATE
    TestDirectTestMode.cpp
    TestBondBlobPersistence.cpp
    TestBondStorageSynchronizer.cpp
    TestGapAdvertisingData.cpp
    TestGapBonding.cpp
    TestGapCentral.cpp
    TestGapPairing.cpp
    TestGapPeripheral.cpp
    TestGapProto.cpp
    TestLinkConfiguringGapCentral.cpp
    TestGattProto.cpp
    TestGapTypes.cpp
    TestGattTypes.cpp
    TestGattClient.cpp
    TestGattClientCharacteristic.cpp
    TestClaimingGattClientConnection.cpp
    TestGapAdvertisementFormatter.cpp
    TestGattServer.cpp
    TestRetryingGattClientConnection.cpp
)
