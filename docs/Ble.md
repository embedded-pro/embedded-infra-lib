# BLE (Bluetooth Low Energy)

## Introduction

The `services/ble` package provides the Generic Access Profile (GAP), the Generic Attribute Profile (GATT) and Direct Test Mode (DTM). This chapter describes the GAP interfaces: how they are split per role, and how their procedures report their outcome.

## Roles

The Bluetooth specification separates what a device may do by the role it plays in a connection. The interfaces follow that separation, so a device includes only what its role needs:

| Header | Contents |
| --- | --- |
| `GapTypes.hpp` | Value types shared by both roles: `GapAddress`, `GapState`, `GapConnectionParameters`, `GapAdvertisingReport`, `GapRequestStatus` |
| `GapAdvertisingData.hpp` | `GapAdvertisingDataParser` and `GapAdvertisementFormatter` |
| `GapPeripheral.hpp` | `GapPeripheral`, its observer and its decorator: advertising and standby |
| `GapCentral.hpp` | `GapCentral`, its observer and its decorator: scanning, connecting and disconnecting |
| `GapPairing.hpp` | `GapPairing`, its observer and its decorator: the Security Manager procedures, used by both roles |
| `GapBonding.hpp` | `GapBonding`, its observer and its decorator: stored bonds, used by both roles |

`GapCentral.hpp` and `GapPeripheral.hpp` do not include each other.

## Asynchronous procedures

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

## Pairing outcomes

Pairing can be started locally, with `PairAndBond`, or by the peer. Both paths report through the same vocabulary, `GapPairingResult`:

- Pairing that this device started reports to the `onDone` of the procedure that started it.
- Pairing that the peer started, and any outcome an observer wants to react to, is reported through `GapPairingObserver::PairingSuccessfullyCompleted` and `GapPairingObserver::PairingFailed`.

## ECHO interface

`GapCentral.proto` and `GapPeripheral.proto` mirror the role split. Each file is self contained and imports nothing but `EchoAttributes.proto`, so each declares the messages its own services use. They live in the packages `gap.central` and `gap.peripheral`, which keeps the two sets of messages apart in generated C++, C# and Java, and lets both descriptor sets be loaded into one pool.

Because the files are self contained, the messages they have in common, such as `Address` and `AddressType`, exist in both and must be kept in step by hand.

An ECHO method cannot return a value, so both stages of a procedure come back over the response service as a `Completion`:

```proto
message Completion
{
    RequestStatus requestStatus = 1;
    Result result = 2;
}
```

A rejected request reports its reason in `requestStatus` and leaves `result` unset. An accepted request reports `accepted` together with the outcome in `result`. Every request has its own completion method, except where a typed response already carries the outcome, such as `ResolvedPrivateAddress` or `IdentityAddress`.

Method ids are never reused for a different meaning. When a method's payload changes, it moves to a new id and the old id is left unused, so that a peer speaking an older version of the protocol fails on an unknown method rather than misreading the new one.
