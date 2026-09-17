add_library(services.ble ${EMIL_EXCLUDE_FROM_ALL} STATIC)

protocol_buffer_echo_all(services.ble GapCentral.proto)
protocol_buffer_csharp(services.ble GapCentral.proto)
protocol_buffer_java(services.ble GapCentral.proto)

protocol_buffer_echo_all(services.ble GapPeripheral.proto)
protocol_buffer_csharp(services.ble GapPeripheral.proto)
protocol_buffer_java(services.ble GapPeripheral.proto)

protocol_buffer_echo_all(services.ble Gatt.proto)
protocol_buffer_csharp(services.ble Gatt.proto)
protocol_buffer_java(services.ble Gatt.proto)

protocol_buffer_echo_all(services.ble Dtm.proto)
protocol_buffer_csharp(services.ble Dtm.proto)
protocol_buffer_java(services.ble Dtm.proto)

target_link_libraries(services.ble PUBLIC
    hal.interfaces
    infra.util
    services.tracer
)

target_sources(services.ble PRIVATE
    Att.hpp
    BondBlobPersistence.cpp
    BondBlobPersistence.hpp
    BondStorageSynchronizer.cpp
    BondStorageSynchronizer.hpp
    GapAdvertisingData.cpp
    GapAdvertisingData.hpp
    GapBonding.cpp
    GapBonding.hpp
    GapCentral.cpp
    GapCentral.hpp
    GapPairing.cpp
    GapPairing.hpp
    GapPeripheral.cpp
    GapPeripheral.hpp
    GapTypes.cpp
    GapTypes.hpp
    GattTypes.cpp
    GattTypes.hpp
    GattClientCharacteristic.cpp
    GattClientCharacteristic.hpp
    GattClientConnection.cpp
    GattClientConnection.hpp
    ClaimingGattClientConnection.cpp
    ClaimingGattClientConnection.hpp
    GattServer.cpp
    GattServer.hpp
    GattServerCharacteristicImpl.cpp
    GattServerCharacteristicImpl.hpp
    RetryingGattClientConnection.cpp
    RetryingGattClientConnection.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
