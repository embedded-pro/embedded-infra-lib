# BLE (Bluetooth Low Energy)

## Introduction

The `services/ble` package provides the Generic Access Profile (GAP) and the Generic Attribute Profile (GATT). This chapter describes those interfaces: how they are split per role and per connection, and how their procedures report their outcome.

## Scope and boundaries

`services/ble` is not a Bluetooth stack. It is an abstraction over a vendor controller and host stack, and it has no consumers inside this repository — it exists to be implemented by downstream ports. What it deliberately leaves to the layer below is recorded here, so that an absence can be told apart from a gap.

**HCI and the Link Layer** are out of scope. No opcodes, no transport, no Link Layer control PDUs. Only the vocabulary appears, in the connection state enums.

**Direct Test Mode** is part of this package, as `services::BleDtm`. It used to sit in `hal/` while its ECHO
service sat here, which left the two halves of one interface in different layers. It is a Bluetooth procedure
described by the Core Specification, Volume 6, Part F, not a hardware abstraction, and like everything else
here it is asynchronous: each procedure returns a `DtmRequestStatus` and reports through `onDone`. Its channel
numbers, packet payloads and PHYs are the ones the specification defines. The unmodulated carrier it also
exposes is *not* Direct Test Mode — controllers offer it through vendor-specific commands — and is named so
that this is visible.

**L2CAP is a non-goal.** LE Credit Based Flow Control channels, connection-oriented channels and signalling
are all delegated to the vendor stack and are not modelled here. This is a decision rather than an oversight:
a port that needs a CoC uses its stack's own API for it. The connection-parameter update path, which an
application does need, is served by `GapPeripheral::RequestConnectionParameterUpdate` on one side and
`GapCentral::UpdateConnectionParameters` on the other. The names say which of the two decides: the central
sets the parameters, in `CONNECT_IND` at establishment and by the connection update procedure afterwards, and
a peripheral can only ask.

**Enhanced ATT (EATT)** follows from that. It is multiple concurrent L2CAP CoC bearers per connection, so it presupposes the L2CAP layer above, and it would turn the single claim per connection described under GATT into a pool with a bearer-selection policy. It is not modelled.

**Extended and periodic advertising** are not modelled. Doing so properly means advertising sets, sync establishment and transfer, primary and secondary PHY, ADI and SID,
and payload reassembly across chained reports: that is a second GAP rather than a widening of the existing one, and with no in-tree consumer and no controller, every test
for it here would assert only that a mock was called with the arguments it was called with. What the module does do is not foreclose it: `GapAdvertisingReport::data` is a
view rather than a member bounded to the 31 bytes of a legacy PDU, so the type system no longer commits to legacy advertising.

**PHY selection** waits on that. Its three honest homes are an extended advertising report, extended scan parameters and a connection-level PHY update procedure. The
first two are deferred above; the third has no home either, because this module has no per-connection GAP object — `GapCentral` *is* the connection. Adding the enum
alone, with nothing consuming it, would be a constant that encodes an assumption and is never used. The one exception is `GapPhy`, which exists because `GapDataLength`
genuinely needs it: the maximum transmission time differs between the 1M and Coded PHYs.

**Robust caching policy** is not modelled, though its primitives are. See *Cache coherence* below.

### Citing the specification

Any constant that claims a specification value carries a comment naming its source, in the form `// Vol 3, Part F, section 3.4.1.1` or `// Assigned Numbers, section 3.7`. A bare literal is indistinguishable from a guess, and a reviewer has no way to check it or to notice when an edit makes it wrong.

The comment must name a source that actually assigns the value. A section that describes a concept without giving it an encoding is not a value source, and citing one is worse than citing nothing, because it invites a reader to trust a number the specification never fixed. Where values are this library's own, the comment says so.

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
auto status = gapCentral.Connect(GapAddress{ macAddress, GapDeviceAddressType::publicAddress }, std::chrono::seconds(10),
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

Procedures that only read locally held state stay synchronous: `GetAddress`, `GetIdentityAddress`, `GetAdvertisementData`, `GetScanResponseData`, `ResolvePrivateAddress`, `GetNumberOfBonds`, `GetMaxNumberOfBonds`, `IsDeviceBonded` and `BondStrength`.

### Connection and advertising parameters

A procedure that leaves a radio parameter unnamed does not avoid choosing it — it lets each port choose, differently and invisibly. Three of them are
therefore named at the call:

- `GapCentral::Connect` takes a `GapConnectionParameters`. The initiator is what carries the interval, the peripheral latency and the supervision timeout in
  `CONNECT_IND`, so this is the role that decides them; `GapCentral::UpdateConnectionParameters` changes them afterwards, and
  `GapPeripheral::RequestConnectionParameterUpdate` only asks. `SupervisionTimeoutIsLongEnough` checks the relation the specification requires between the
  three, which is the one way to get a set of parameters that the controller will take and the link will not survive.
- `GapPeripheral::Advertise` takes a `GapAdvertisingParameters`: the type, the interval, the primary channels to advertise on, and the filter policy deciding
  whose scan and connection requests are acted on.
- `GapCentral::StartDeviceDiscovery` takes a `GapScanParameters`: interval, window and whether scanning is active or passive.

Each has a documented default — `defaultConnectionParameters`, `defaultAdvertisingParameters`, `defaultScanParameters` — reachable through a shorter
overload, so that the common case stays short and the default is one the module states rather than one each port invents.

### Pairing outcomes

Pairing can be started locally, with `PairAndBond`, or by the peer. Both paths report through the same vocabulary, `GapPairingResult`:

- Pairing that this device started reports to the `onDone` of the procedure that started it.
- Pairing that the peer started, and any outcome an observer wants to react to, is reported through `GapPairingObserver::PairingSuccessfullyCompleted` and `GapPairingObserver::PairingFailed`.

### Bond strength

Whether a device is bonded is rarely the question an application actually has. Before trusting a bonded peer with a privileged operation it needs to know how much that
bond is worth: whether it came from LE Secure Connections or legacy pairing, whether its key is authenticated, and how large the key is. Mode 1 Level 4 exists precisely
to mean "authenticated LESC with a 128-bit key", so those distinctions are the point rather than detail.

`GapBondStrength` carries all three. `GapBonding::BondStrength` returns it, absent when the device is not bonded, and `GapPairingObserver::PairingSuccessfullyCompleted` reports the same structure so an application that has just paired learns what it got without asking again.

`GapPairing::SecurityModeAndLevel` is one closed enum of exactly the six combinations the specification defines, so a mode and level that do not go together — Mode 2 stops at level 2 — is a compile error rather than a request answered with `notSupported`. Secure Connections Only is not among them: it is a property of the device rather than of one link, and has its own procedure.

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
has to watch a GAP role to notice a disconnection. The link going away is reported to
`GattClientObserver::ConnectionReleased`, which is where an application that kept the
`infra::SharedPtr` it was handed lets go of it; until it does, the connection holds its slot.

An indication is acknowledged exactly once. Every `GattClientUpdateObserver` on a connection
sees every indication, and a `GattClientCharacteristic` that the handle does not belong to
finishes immediately, so whoever hands an indication to the observers must give each of them
its own completion and report upwards only after the last one is done. `GattIndicationFanOut`
does that, and both the decorator and a stack implementation deliver through it. Acknowledging
per observer instead would send one `IndicationDone` per characteristic, and would confirm the
indication before the characteristic it belongs to had finished with it.

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
all. Its only outcome is whether the stack took it, which the caller reads from the returned
status. `RetryingGattClientConnection` decorates a connection to re-attempt it while the
stack answers `busy`, on a timer so that a stack which stays busy cannot monopolise the event
dispatcher, and the timer abandons a pending attempt when the decorator is destroyed. Any
other refusal is the caller's to see, so it is returned unchanged rather than retried.

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

### Long reads and writes

A characteristic value larger than `ATT_MTU - 3` takes several round trips: Read Blob for reading, and Prepare Write followed by Execute Write for writing. `GattClientConnection` carries those three as primitives, one ATT PDU each, so the status and `onDone` contract above holds for them unchanged.

Composing them is not a caller's job. `GattClientLongOperations` publishes `ReadLong` and `WriteLong`, and `ClaimingGattClientConnection` implements it — deliberately
there rather than above it, because the sequence must hold **one claim for its whole duration**. A prepare queue belongs to the bearer, not to the caller: releasing the
claim between two Prepare Writes would let a second caller queue a fragment of its own, which this connection's Execute Write would then commit. That is silent data
corruption rather than a visible failure, so the claim boundary is the design. Holding the claim also settles the chunk size, since `ExchangeMtu` claims the same resource
and the MTU therefore cannot change mid-sequence.

A port whose stack performs long reads and writes natively implements `GattClientLongOperations` directly instead of having its primitives driven from here.

Buffers are the caller's. `ReadLong` fills an `infra::BoundedVector<uint8_t>` sized to what that characteristic holds, so no object here grows by a byte and the abstraction never guesses a size; a value that does not fit reports `insufficientResources` with the buffer full. `WriteLong` copies nothing, and its `data` must outlive the procedure.

Three details the specification decides rather than taste: a value short enough to fit one request does not open a prepare queue at all; every echoed offset and value is
verified and a mismatch cancels the queue, because a peer must not be able to commit something other than what was sent; and any failure mid-sequence cancels the queue
and reports the **original** failure, discarding the cancel's own outcome, which has nothing to add that the first failure did not.

Read Multiple is deliberately absent. Its response is a bare concatenation with no length delimiters, so a caller can decode it only by already knowing every value's length — which is why Core 5.2 had to add a variable-length variant. Shipping the 4.0 form would ship a decoding trap.

### Cache coherence

A client needs three things to notice that a peer's attribute database has changed: the `databaseOutOfSync` result, the Service Changed characteristic and the Database
Hash characteristic. All three are here — the result in `GattResult`, the UUIDs in the `uuid` namespace, and `GattServiceChangedFromValue` to decode the four-byte value.
Service Changed needs no procedure of its own: a client subscribes with `EnableIndication` and receives it through `GattClientUpdateObserver::IndicationReceived` like any
other indication.

The **policy** is deliberately not here. How much to re-discover on seeing a change, whether to do it eagerly or lazily, and what to do with handles already in flight are
application decisions, and persisting a peer's Database Hash needs the bond store, which is a device-scoped object with a different lifetime from a connection. Building
that machine into `GattClientConnection` would make a connection-scoped object depend on a device-scoped one and would bake one caching policy into a contract whose whole
purpose is that ports differ.

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
