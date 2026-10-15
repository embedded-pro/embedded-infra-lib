add_library(services.ble_proto ${EMIL_EXCLUDE_FROM_ALL} STATIC)

protocol_buffer_echo_all(services.ble_proto GapCentral.proto)
protocol_buffer_csharp(services.ble_proto GapCentral.proto)
protocol_buffer_java(services.ble_proto GapCentral.proto)

protocol_buffer_echo_all(services.ble_proto GapPeripheral.proto)
protocol_buffer_csharp(services.ble_proto GapPeripheral.proto)
protocol_buffer_java(services.ble_proto GapPeripheral.proto)

protocol_buffer_echo_all(services.ble_proto GattClient.proto)
protocol_buffer_csharp(services.ble_proto GattClient.proto)
protocol_buffer_java(services.ble_proto GattClient.proto)

protocol_buffer_echo_all(services.ble_proto GattServer.proto)
protocol_buffer_csharp(services.ble_proto GattServer.proto)
protocol_buffer_java(services.ble_proto GattServer.proto)

protocol_buffer_echo_all(services.ble_proto Dtm.proto)
protocol_buffer_csharp(services.ble_proto Dtm.proto)
protocol_buffer_java(services.ble_proto Dtm.proto)
