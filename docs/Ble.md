# BLE (Bluetooth Low Energy)

## Introduction

The `services/ble` package provides the Generic Access Profile (GAP), the Generic Attribute Profile (GATT) and Direct Test Mode (DTM). This chapter describes the GAP and GATT interfaces: how they are split per role and per connection, and how their procedures report their outcome.

## GAP

### Roles

The Bluetooth specification separates what a device may do by the role it plays in a connection. The interfaces follow that separation, so a device includes only what its role needs:

| Header                   | Contents                                                                                                              |
|--------------------------|-----------------------------------------------------------------------------------------------------------------------|
| `GapTypes.hpp`           | Value types shared by both roles: `GapAddress`, `GapConnectionParameters`, `GapAdvertisingReport`, `GapRequestStatus` |
| `GapAdvertisingData.hpp` | `GapAdvertisingDataParser` and `GapAdvertisementFormatter`                                                            |
| `GapPeripheral.hpp`      | The advertising role: `GapPeripheral`, its observer, its decorator and `GapPeripheralState`                           |
| `GapCentral.hpp`         | The scanning and connecting role: `GapCentral`, its observer, its decorator and `GapCentralState`                     |
| `GapPairing.hpp`         | `GapPairing`, its observer and its decorator: the Security Manager procedures, used by both roles                     |
| `GapBonding.hpp`         | `GapBonding`, its observer and its decorator: stored bonds, used by both roles                                        |

`GapCentral.hpp` and `GapPeripheral.hpp` do not include each other.

The link layer states a device passes through differ per role, so each role has its own state type. A peripheral moves between `standby`, `advertising` and `connected`; a central moves between `standby`, `scanning`, `initiating` and `connected`. Neither role can observe a state it cannot reach.

### Asynchronous procedures

Every GAP procedure is a controller operation. It cannot report its outcome at the point of the call, because the controller has not done the work yet. Each procedure therefore has two results:

- Its **return value**, a `GapRequestStatus`, says whether the request was accepted at all.
- Its **completion callback**, an `infra::Function` passed as the last parameter, says how the procedure ended.

```cpp
enum class GapRequestStatus : uint8_t
{
    accepted = 0,
    invalidState,
    invalidParameter,
    busy,
    notSupported
};
```

```cpp
auto status = gapCentral.Connect(macAddress, GapDeviceAddressType::publicAddress, std::chrono::seconds(10),
    [this](GapCentral::Result result)
    {
        if (result == GapCentral::Result::success)
            StartDiscovery();
    });
```

The rules an implementation follows:

- A request that is not accepted never results in a call to `onDone`. A caller that gets anything other than `accepted` back is done with that request.
- `onDone` is never invoked from within the call itself. Implementations schedule it with `infra::EventDispatcher::Instance().Schedule()`, so a caller never re-enters itself through its own callback. See [Execution model](ExecutionModel.md).
- Where the callback outlives the object that owns it, the implementation guards it with an `infra::WeakPtr`, so that the action is discarded if that object has expired.

Each role has its own result type, because the ways a procedure can end differ per role: `GapCentral::Result` distinguishes `cancelled`, `timeout` and `connectionFailed`; `GapPeripheral::Result` distinguishes `invalidParameter` and `controllerError`; pairing reports a `GapPairingResult`.

Procedures that only read locally held state stay synchronous: `GetAddress`, `GetIdentityAddress`, `GetAdvertisementData`, `GetScanResponseData`, `ResolvePrivateAddress`, `GetNumberOfBonds`, `GetMaxNumberOfBonds` and `IsDeviceBonded`.

### Pairing outcomes

Pairing can be started locally, with `PairAndBond`, or by the peer. Both paths report through the same vocabulary, `GapPairingResult`:

- Pairing that this device started reports to the `onDone` of the procedure that started it.
- Pairing that the peer started, and any outcome an observer wants to react to, is reported through `GapPairingObserver::PairingSuccessfullyCompleted` and `GapPairingObserver::PairingFailed`.

### ECHO interface

`GapCentral.proto` and `GapPeripheral.proto` mirror the role split. Each file is self contained and imports nothing but `EchoAttributes.proto`, so each declares the messages its own services use. They live in the packages `gap.central` and `gap.peripheral`, which keeps the two sets of messages apart in generated C++, C# and Java, and lets both descriptor sets be loaded into one pool.

Because the files are self contained, the messages they have in common, such as `Address` and `AddressType`, exist in both and must be kept in step by hand.

An ECHO method cannot return a value, so both stages of a procedure come back over the response service as a completion message:

```proto
message Completion
{
    RequestStatus requestStatus = 1;
    Result result = 2;
}
```

A rejected request reports its reason in `requestStatus` and leaves `result` unset. An accepted request reports `accepted` together with the outcome in `result`. Every request has its own completion method, except where a typed response already carries the outcome, such as `ResolvedPrivateAddress` or `IdentityAddress`.

The completion message a method carries is the one that matches the result its C++ counterpart reports, so no outcome is lost in the crossing. `Completion` carries the role's own result, and its `Result` enum holds exactly the values of `GapPeripheral::Result` or `GapCentral::Result` in the file it belongs to. Pairing procedures carry a `PairingCompletion`, whose result is the `GapPairingResult` vocabulary. Bond removal carries a `BondCompletion`, which has no result, because `GapBonding` reports its outcome through `NumberOfBondsChanged`.

Method ids are never reused for a different meaning. When a method's payload changes, it moves to a new id and the old id is left unused, so that a peer speaking an older version of the protocol fails on an unknown method rather than misreading the new one.

## GATT

### Interfaces

| Header                         | Contents                                                                                                                                                    |
|--------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Att.hpp`                      | `AttAttribute`, `AttErrorCode` and `attDefaultMaxMtuSize`                                                                                                   |
| `GattTypes.hpp`                | The attribute value types `GattDescriptor`, `GattCharacteristic` and `GattService`, plus `GattRequestStatus`, `GattResult` and `GattResultFromAttErrorCode` |
| `GattClientConnection.hpp`     | `GattClientConnection`, its two observers and `GattClientConnectionDecorator`                                                                               |
| `GattClient.hpp`               | `GattClient` and `GattClientObserver`: the connections a client holds                                                                                       |
| `GattClientCharacteristic.hpp` | `GattClientCharacteristic` and `GattClientService`: the application facing model of a discovered database                                                   |
| `GattServer.hpp`               | The server side: `GattServerService`, `GattServerCharacteristic` and `GattServerDescriptor`                                                                 |

### One object per connection

A GATT client holds several connections at once, and every operation belongs to the
connection it is performed on. `GattClient` reports each one to its observers:

```cpp
void ConnectionEstablished(infra::SharedPtr<GattClientConnection> connection) override
{
    connection->DiscoverServices([this](GattResult result)
        {
            // ...
        });
}
```

Each `GattClientConnection` is its own `infra::Subject`, so an observer attached to one
connection never sees the discovery results or the updates of another. This matters because
ATT handles are only unique within one peer's attribute database: routing updates by handle
alone would deliver a notification from one peer to a characteristic discovered on another.
For the same reason `ClaimingGattClientConnection` holds one `infra::ClaimableResource` per
connection — ATT serialises per link, so two peers never queue behind each other.

Connections are pre-allocated by the stack, and `MaxNumberOfConnections` reports that bound.
A connection stays alive as long as a `infra::SharedPtr` to it is held, so its slot is reused
only once the last holder lets go, and an action that may outlive its connection holds an
`infra::WeakPtr` and is discarded if the connection has expired.

A connection completes its outstanding operations when its link is lost, reporting
`GattResult::disconnected`. That is what releases the claims a decorator holds, so nothing
has to watch a GAP role to notice a disconnection.

### Asynchronous operations

GATT follows the same two stage contract as GAP. Each operation returns a
`GattRequestStatus` saying whether the request was accepted, and reports how it ended
through an `infra::Function` passed as its last parameter:

```cpp
auto status = connection->Read(valueHandle,
    [this](GattResult result, infra::ConstByteRange data)
    {
        if (result == GattResult::success)
            Process(data);
    });
```

The rules an implementation follows are those of the GAP procedures: a request that is not
`accepted` never results in a call to `onDone`; `onDone` is never invoked from within the
call itself; and a callback that outlives the object owning it is guarded with an
`infra::WeakPtr`.

Discovery reports its results over `GattClientConnectionObserver`, because there are many of
them, and reports its completion through the `onDone` of the procedure that started it:

```cpp
connection->DiscoverCharacteristics(service, [this](GattResult result)
    {
        // every CharacteristicDiscovered for this service has arrived
    });
```

`WriteWithoutResponse` has no ATT response to wait for, so it has no completion callback at
all. Its only outcome is whether the stack took it, which is `accepted` or `busy`;
`RetryingGattClientConnection` decorates a connection to re-attempt it while it is `busy`.

`EffectiveMaxAttMtuSize` reads locally held state and stays synchronous.

### Result vocabulary

```cpp
enum class GattRequestStatus : uint8_t
{
    accepted = 0,
    invalidState,
    invalidParameter,
    busy,
    notSupported
};
```

`GattResult` says how a procedure ended. Its values are the ways an ATT procedure can fail,
plus `disconnected` and `timeout`, which have no ATT error code because they are produced
locally. A stack translates the byte it receives with `GattResultFromAttErrorCode`, so that
the mapping from Core Specification Volume 3, Part F, section 3.4.1.1 lives in one place
rather than in each caller.

### GATT ECHO interface

`GattClient.proto` and `GattServer.proto` mirror the split, in the packages `gatt.client`
and `gatt.server`. Each file is self contained and imports nothing but
`EchoAttributes.proto`.

Every client message carries a `ConnectionId`, so that one ECHO link drives several
connections, and the response service reports `ConnectionEstablished` and
`ConnectionReleased`. Completions take the same shape as the GAP ones:

```proto
message Completion
{
    ConnectionId connection = 1;
    RequestStatus requestStatus = 2;
    Result result = 3;
}
```

`Result` holds exactly the values of `services::GattResult` and `RequestStatus` those of
`services::GattRequestStatus`. As in GAP, a method id is never reused for a different
meaning: when the payloads gained their `ConnectionId` every method moved to a new id, and
the ids of the single connection interface are left unused.

A message may not be called `Descriptor`: protoc generates a static `Descriptor` member on
every generated C# message, which a message of that name would collide with. `Descriptr`
keeps its spelling for that reason.
