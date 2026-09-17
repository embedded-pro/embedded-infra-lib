# BLE Conformance Assessment

An assessment of how closely `services/ble` follows the Bluetooth Core Specification, and
where it departs from it.

Assessed at commit `5ff138b`. Line references are to that commit and drift with later edits.
Verified against a green build: `services.ble_test` builds clean under the `host` preset and
all 206 tests pass.

## Why this assessment exists

`services/ble` is roughly 2,400 lines of production C++ across eighteen headers, covering the
Generic Access Profile, a GATT client and server object model, pairing and bonding, and five
ECHO `.proto` surfaces. It has been rewritten twice in recent history: `2cb78cd` split GAP into
central and peripheral roles with asynchronous, status-returning operations, and `5ff138b`
moved the GATT client to a per-connection architecture.

It also has no consumers inside this repository. Searching for `#include "services/ble/` outside
the module returns nothing; the only things that link `services.ble` are its own tests and test
doubles. The module exists to be implemented by downstream vendor-stack ports.

That combination is what makes this assessment worth doing. A freshly rewritten API with no
in-tree users gets no corrective pressure from real integration, so a deviation from the
specification cannot show up as a build failure here. It shows up later, in somebody else's
port, as an interop bug. And because the module *is* the contract those ports code against,
every place its vocabulary diverges from the specification is a place each port author has to
translate by hand. Finding 2 below shows that translation already going wrong, in this
repository, between two files that are meant to describe the same enum.

## How the score is derived

`services/ble` is not a Bluetooth stack. It is an abstraction over a vendor controller and host
stack, so marking it down for not implementing HCI or the Link Layer would measure the wrong
thing. Each layer is therefore scored on two axes:

| Axis | Question |
| --- | --- |
| **Fidelity** | Where the module *does* model a specification concept, does it match the spec's semantics, terminology, value ranges and assigned numbers? |
| **Coverage** | Of the concepts a portable BLE abstraction is expected to expose, how many are exposed at all? |

A layer that is deliberately delegated to the vendor stack scores `N/A` for coverage and is left
out of the weighted total — but only where the delegation is explicit, either documented or
structurally obvious. Direct Test Mode qualifies: it lives in `hal::BleDtm`, outside this module,
which is a positive design decision. Simply being absent does not qualify. That distinction is
what keeps the L2CAP score honest.

Layer score is `Fidelity x 0.6 + Coverage x 0.4`, each axis out of 10.

| Severity | Meaning |
| --- | --- |
| **S1** | Contradicts the specification: a value disagreeing with Assigned Numbers, an API permitting what the spec forbids, or a wire format that depends on host endianness. |
| **S2** | Misleading naming: a spec term reused for a non-spec meaning, or a spec concept given a non-spec name. |
| **S3** | Missing concept: something mandatory in practice has no representation, forcing callers to bypass the abstraction. |
| **S4** | Incomplete range: the concept exists but its value domain is narrower than the spec's. |
| **S5** | Cosmetic drift with no functional consequence. |

This assessment is version-agnostic. Rather than pinning one Core Specification release, each
finding notes the version that introduced the feature where that is relevant.

## Scorecard

| Layer | Weight | Fidelity | Coverage | Score |
| --- | --- | --- | --- | --- |
| GATT / ATT | 30% | 8.5 | 5.0 | **7.1** |
| GAP | 25% | 5.5 | 4.0 | **4.9** |
| Security Manager (pairing and bonding) | 20% | 5.5 | 4.5 | **5.1** |
| L2CAP | 10% | — | 2.0 | **2.0** |
| HCI / Link Layer | 5% | 8.0 | N/A | **8.0** |
| Cross-cutting | 10% | 5.0 | 6.0 | **5.4** |

**Overall: 5.5 / 10** — *recognisably BLE, but terminology and value domains drift far enough
to mislead.*

Bands: 9.0–10 spec-faithful · 7.0–8.9 aligned, deviations deliberate · 5.0–6.9 recognisably BLE
but drifting · 3.0–4.9 BLE-flavoured, not BLE · 0–2.9 nominal resemblance.

### The pattern behind the number

The single most useful result of this assessment is not the score. It is this:

> **Fidelity is high wherever the module cites the specification, and drops sharply wherever it
> does not.**

Three files carry a Core Specification citation — `Att.hpp:22`, `GattTypes.hpp:84` and
`GattServer.hpp:89` — and in two of them every value is exact and verifiable. The ATT error
codes, the characteristic property flags, the CCCD UUID and its values, the ten Device
Information Service UUIDs, the advertising interval range and unit: all correct.

Nearly every significant deviation recorded below is in an uncited constant. That makes the
remedy concrete rather than cultural: requiring a `// Vol x, Part y, section z` comment beside
any constant that claims a spec value would have caught most of this list at review time. It
would also have caught the one case where a citation is present but does not mean what it
appears to mean (finding 20).

## GATT and ATT — 7.1

The strongest part of the module, and the part where the design shows genuine specification
insight rather than mechanical translation.

The per-connection architecture introduced in `5ff138b` is correct for the right reason: ATT
handles are only unique within one peer's attribute database, so routing notifications by
handle alone would deliver an update from one peer to a characteristic discovered on another.
`ClaimingGattClientConnection` holds one `infra::ClaimableResource` per connection, which
correctly models ATT's one-outstanding-request-per-bearer rule without letting two peers queue
behind each other. `GattIndicationFanOut` acknowledges an indication exactly once regardless of
how many local observers are attached, which is the correct reading of Handle Value
Confirmation. `GattServer.cpp`'s attribute accounting — two attributes per characteristic, three
when notify or indicate is set, one per service — is correct bookkeeping.

Values are exact throughout. `AttErrorCode` (`Att.hpp:24-46`) matches Vol 3 Part F §3.4.1.1 for
the whole core range 0x00–0x13. `GattCharacteristic::PropertyFlags` (`GattTypes.hpp:86-97`)
matches Vol 3 Part G §3.3.1.1. The CCCD attribute type 0x2902 and its values
(`GattTypes.hpp:41-49`) are right, as are all ten DIS UUIDs (`GattTypes.hpp:67-79`).
`attDefaultMaxMtuSize = 23` is the correct ATT default.

Coverage is what pulls the score down.

**S3 — `GattResult` discards `databaseOutOfSync`.** `GattTypes.cpp:5-38` maps ATT errors onto a
twelve-value `GattResult`. Error 0x12 is not handled and falls through to
`GattResult::unknown`. That code is precisely the signal that a client's cached attribute
database is stale and must be re-discovered. Collapsing it means a client *cannot* implement
correct cache invalidation through this API. This compounds with the absence of Service Changed
(0x2A05), Database Hash (0x2B2A), and any robust-caching support: the whole cache-coherence
mechanism introduced in Core 5.1 has no representation. Note that the lossy mapping is otherwise
deliberate and defensible — merging `readNotPermitted` and `writeNotPermitted` into
`notPermitted` loses little, since the caller knows which operation it issued.

**S3 — no long reads or writes.** There is no Prepare Write plus Execute Write, no Read Blob, no
Read Multiple, and no Read By Type by UUID. Any characteristic value larger than `ATT_MTU - 3`
can be neither written nor fully read through this API. For a default 23-byte MTU that is a
20-byte ceiling. This is a hard functional limit, not a missing convenience.

**S3 — no included services.** `GattService` has no notion of a 0x2802 Include declaration, so
the specification's service-composition mechanism is unavailable, and
`GattClientConnection` offers no way to discover one.

**S3 — core declaration UUIDs are absent.** The `uuid` namespace defines ten DIS UUIDs and
nothing else. There is no 0x2800 Primary Service, 0x2801 Secondary Service, 0x2802 Include,
0x2803 Characteristic, no 0x2900/0x2901/0x2903/0x2904/0x2905 descriptor types, and no 0x1800
GAP or 0x1801 GATT service. A GATT abstraction that lacks the declaration UUIDs its own
attribute model is built from is incomplete, and every port will redefine them locally.

**S4 — `valueHandleOffset = 1`** (`GattTypes.hpp:41`) encodes the assumption that a Client
Characteristic Configuration descriptor sits at the value handle plus one. The specification
does not guarantee that; other descriptors may intervene. The constant is **declared and never
used** — it has exactly one occurrence in the tree, its own declaration — and the test suite
explicitly rejects the rule it encodes. `GattClientCharacteristicWithDistantValueHandleTest`
contains `notification_just_past_the_declaration_handle_is_ignored`: an update arriving at
declaration handle plus one, which is exactly the handle this constant computes, must be
discarded. Dead code enshrining an assumption the tests disprove by name: cheap to delete,
actively misleading if kept.

**S4 — `GetAttributeCount()` returns `uint8_t`** (`GattTypes.hpp:149`, and the `GattServer`
overrides), capping a service at 255 attributes where ATT handles run to 0xFFFF.

**S4 — `GattServerCharacteristic::PermissionFlags`** (`GattServer.hpp:89-100`) carries the
comment "Description in Bluetooth Core Specification, Volume 3, Part F, section 3.2.5", but the
bit values are invented by this library. The citation reads like a value source and is not one —
the one case in the module where a citation is actively misleading rather than merely absent.
`GattServerDescriptor::AccessFlags` is likewise invented and uncited.

Also absent: signed writes, and Enhanced ATT (EATT, Core 5.2) in any form.

## GAP — 4.9

Where the module's vocabulary drifts furthest from the specification.

The good parts first. The Link Layer state enums are correct and correctly split: a peripheral
has `standby`/`advertising`/`connected`, a central adds `scanning`/`initiating`, and neither
role can observe a state it cannot reach. `GapAdvertisementFlags` (`GapTypes.hpp:45-52`) matches
the LE Flags bits exactly. The advertising interval is modelled well — `GapPeripheral.hpp:32-34`
documents its unit (`Interval = Multiplier * 0.625 ms`) and its range (0x20 to 0x4000), which
matches HCI LE Set Advertising Parameters. The AD parser and formatter are the only genuine wire
codec in the module and handle malformed input carefully.

That makes the connection-parameter struct all the more conspicuous, because it does none of
those things.

**S2 — `GapConnectionParameters` departs from spec vocabulary on every field.**
(`GapTypes.hpp:66-75`)

| Field | Specification |
| --- | --- |
| `minConnIntMultiplier` / `maxConnIntMultiplier` | Connection Interval Min / Max, in units of 1.25 ms, range 0x0006–0x0C80. "Multiplier" is invented, and unlike the advertising interval two files away, neither the unit nor the range is documented anywhere. |
| `slaveLatency` | **Peripheral Latency.** The SIG retired "slave" in Core 5.3's inclusive-terminology change. This module was *just rewritten* around central and peripheral roles, so the field contradicts its own module's vocabulary. |
| `supervisorTimeoutMs` | **Supervision** Timeout, in units of **10 ms**, range 0x000A–0x0C80. |

The supervision timeout is the serious one, and is **S1** rather than S2. The `Ms` suffix
asserts milliseconds; the specification's field is in units of 10 ms. Either this field deviates
from spec units or its name is wrong, and a port author has no way to tell which. A tenfold
error in supervision timeout is a spuriously dropped or stubbornly persistent connection. The
inconsistency with `AdvertisementIntervalMultiplier`, which *does* document its unit, is what
makes this a guess rather than a convention.

**S3 — Data Length Extension constants are filed under connection parameters.**
`connectionInitialMaxTxOctets = 251` and `connectionInitialMaxTxTime = 2120`
(`GapTypes.hpp:73-74`) are LE Data Length Extension values, exchanged by a different procedure
than connection parameters. The arithmetic is right, but `(251 + 14) * 8` is the 1M PHY figure;
on LE Coded PHY the maximum is 17040 µs. Hardcoding 2120 silently assumes 1M.

**S3 — advertising is asymmetric between transmit and receive.** `GapAdvertisementType`
(`GapTypes.hpp:17-21`) offers only `advInd` and `advNonconnInd`, while the receive-side
`GapAdvertisingEventType` (`GapTypes.hpp:23-30`) carries all four PDU types plus scan response.
A peripheral can therefore *observe* directed and scannable-undirected advertising but cannot
*perform* it — which rules out high-duty-cycle directed advertising, the standard mechanism for
fast reconnection to a known central.

**S3 — AD type coverage is thin.** `GapAdvertisementDataType` (`GapTypes.hpp:32-43`) defines
eight of the Common Data Types. Missing, among others: Service Data in all three forms
(0x16, 0x20, 0x21) and TX Power Level (0x0A), both heavily used in practice, plus the incomplete
list types (0x02, 0x06). The consequence for the formatter is concrete:
`AppendListOfServicesUuid` can only ever emit a *complete* list, which is a meaningful assertion
to a scanner — "these are all my services" — that the caller cannot opt out of making.

**S2 — `unknownType = 0x00`** (`GapTypes.hpp:34`) is a sentinel sitting inside an enum whose
every other member is a real Assigned Number. 0x00 is not an assigned AD type.

**S3 — no scan parameters.** `StartDeviceDiscovery` takes no scan interval, scan window, or
active-versus-passive choice, so a central cannot trade discovery latency against power, and
cannot decline to send scan requests.

**S3 — no PHY anywhere in GAP.** No 1M, 2M or Coded PHY selection and no PHY update procedure,
although `hal::BleDtm` already carries a `phy` parameter. The HAL models the concept and the
GAP layer does not.

**S4 — Extended Advertising is structurally unrepresentable.** `GapAdvertisingReport::data` is
hard-bounded to 31 bytes (`GapTypes.hpp:101`), and the report carries no TX power, no primary or
secondary PHY, and no periodic advertising interval. The type system commits to legacy
advertising. Advertising sets and periodic advertising are likewise absent.

**S4 — `rssi` is `int32_t`** (`GapTypes.hpp:102`). Specification RSSI is a signed 8-bit value
where 127 means "not available"; that convention has nowhere to live in an `int32_t`.

**S4 — address types are coarse.** `GapDeviceAddressType` (`GapTypes.hpp:11-15`) offers only
`publicAddress` and `randomAddress`. The specification distinguishes resolvable private,
non-resolvable private and static random addresses, and separates device addresses from identity
addresses. `ResolvePrivateAddress` (`GapCentral.hpp:45`) exists, so privacy is partly modelled,
but the type cannot express which kind of random address it holds — and
`ResolvePrivateAddress` returns a bare `std::optional<hal::MacAddress>`, dropping the type of
the identity address it resolved to.

## Security Manager — 5.1

`GapPairing` covers the procedures an application needs to drive: IO capabilities, security mode
and level, passkey entry, numeric comparison, and LE Secure Connections out-of-band data. The
`GapOutOfBandData` struct correctly carries both the random and confirm values, which is right
for LESC OOB. `PairAndBond`'s documented semantics — encrypt if a bond exists, otherwise pair,
encrypt and bond — are an accurate description of the usual flow.

**S1 — the C++ and proto `IoCapabilities` orderings disagree.** This is the most immediately
dangerous finding in the assessment.

| | Values |
| --- | --- |
| `GapPairing.hpp:41-48` | `display=0, displayYesNo=1, keyboard=2, none=3, keyboardDisplay=4` |
| `GapPeripheral.proto:36-43` | `none=0, display=1, displayYesNo=2, keyboard=3, keyboardDisplay=4` |

The C++ ordering happens to match the Security Manager IO Capability encoding of Vol 3 Part H
exactly — DisplayOnly 0x00, DisplayYesNo 0x01, KeyboardOnly 0x02, NoInputNoOutput 0x03,
KeyboardDisplay 0x04 — though nothing in the file says so, and nothing protects it. The proto
ordering matches neither the C++ enum nor the specification.

Any port that casts between the two turns `display` into `none`, which downgrades the pairing
method selection and can silently drop MITM protection. No bridging code exists in this
repository, so today this is latent rather than live — but the whole purpose of the proto files
is to be bridged by downstream ports, and this is a trap laid for each one of them. Verifying
that no downstream port has already hit it is outside what can be checked from this repository.

**S3 — the security model is coarser than the specification's.** `SecurityMode{mode1, mode2}`
(`GapPairing.hpp:50-54`) has no Secure Connections Only mode. `SecurityLevel{level1..level4}`
(`GapPairing.hpp:56-62`) is applied uniformly to both modes, although Mode 2 defines only levels
1 and 2 — so `SetSecurityMode(mode2, level4)` is representable and meaningless, and the type
system offers no help.

**S3 — an application cannot tell how strong an existing bond is.** `GapBonding` exposes
`IsDeviceBonded`, a count, and a maximum. There is no way to ask whether a bond was established
with LE Secure Connections or with legacy pairing, or whether its key is authenticated or
unauthenticated. An application deciding whether to trust a bonded peer with a privileged
operation has no basis for the decision. Given that Mode 1 Level 4 exists precisely to express
"authenticated LESC with a 128-bit key", not surfacing it is a security-relevant blind spot
rather than a coverage nicety.

**S4 — `GapPairingResult` collapses the SM error codes.** Nine values stand in for the roughly
fourteen codes of Vol 3 Part H §3.5.5. Absent distinctions include DHKey check failure, confirm
value failure, repeated attempts, OOB data not available, and cross-transport key derivation not
allowed. Several of those distinguish a benign failure from an active attack, so flattening them
removes the information a security-conscious application would log or act on.

**S5 — `DisplayPasskey(int32_t passkey, bool numericComparison)`** (`GapPairing.hpp:31`) types a
six-digit value of 000000–999999 as signed, and overloads two distinct SM procedures — passkey
display and numeric comparison — onto one callback discriminated by a bare `bool` at the call
site.

No LTK, IRK or CSRK types are exposed, and no key-size negotiation surface exists beyond the
`insufficientEncryptionKeySize` error. For an abstraction that delegates SMP to the vendor
stack this is largely reasonable; the bond-strength gap above is the part that is not.

## L2CAP — 2.0

Entirely absent. There is no LE Credit Based Flow Control channel support, no connection-oriented
channels, and no signalling. Repository-wide, `l2cap` matches nothing in any `.hpp`, `.cpp` or
`.proto`.

Unlike Direct Test Mode, this absence is nowhere documented as a delegation decision, so under
the fairness rule stated above it is scored rather than excused. LE CoC is application-visible
and is the standard mechanism for bulk transfer that would otherwise be forced through
characteristic writes. The connection-parameter-update path is partially served by
`SetConnectionParameters`, which is why the score is not zero.

If L2CAP is in fact a deliberate non-goal, saying so in `docs/Ble.md` would convert this from a
gap into a documented boundary, and the layer would score `N/A`.

## HCI and Link Layer — 8.0

Correctly and deliberately out of scope, and the boundary is drawn well.

No HCI opcodes, no transport, no Link Layer control PDUs. What does appear is the vocabulary:
the LL state enums use the specification's own names, and `GapCentral.proto:20` and
`GapPeripheral.proto:20` cite Vol 6 Part B §1.1 for them. `controllerError` in both role `Result`
enums is an honest acknowledgement that errors originate below this layer. Direct Test Mode is
placed in `hal::BleDtm`, outside this module, which is the right home for it.

Two deductions. The Data Length Extension constants leak Link Layer concerns into
`GapConnectionParameters`, as noted above. And `Dtm.proto` — the only file in the repository
naming a Core Specification version, 5.3 — has no C++ interface binding it to `hal::BleDtm`, so
the RPC surface and the HAL interface it mirrors are maintained independently by hand.

## Cross-cutting — 5.4

### Wire-format discipline is inconsistent

The AD codec serialises and parses multi-byte fields in three different ways, only one of which
is endianness-safe.

`ManufacturerSpecificData()` and `Appearance()` extract through
`infra::ByteInputStream::Extract<uint16_t>()` with soft-fail — byte-wise, explicit, correct.

`CompleteListOf16BitUuids()` (`GapAdvertisingData.cpp:62-68`) instead reinterpret-casts the raw
advertising payload to `const AttAttribute::Uuid16*`. This has two problems, and the second is
the more serious:

1. **Endianness.** Reinterpreting on-air little-endian bytes as a host `uint16_t` yields the
   right value only on a little-endian host.
2. **Alignment.** An AD structure's offset within the payload depends on whatever structures
   precede it, so the resulting `uint16_t*` can land on an odd address. On ARMv6-M an unaligned
   16-bit load faults. This repository ships a preset for exactly that architecture —
   `cortex-m0plus`, "Cortex-M0+ compile coverage (ARMv6-M)" in `CMakePresets.json` — so it is a
   supported target, not a hypothetical one. The fix already exists two functions away.

   The hazard is latent today for a specific reason worth recording: the `cortex-m0plus` preset
   sets `EMIL_INCLUDE_ECHO: Off`, and `services/CMakeLists.txt` only adds the `ble` subdirectory
   when ECHO is on. So `services/ble` is never compiled for ARMv6-M in this repository, and no
   existing CI configuration can catch this. It would surface in a downstream port that enables
   ECHO on a Cortex-M0 or M0+ target.

On the formatter side, `AppendManufacturerData`, `AppendAppearance` and the 16-bit
`AppendListOfServicesUuid` (`GapAdvertisingData.cpp:148-191`) write integers by reinterpreting
host-order bytes. On a little-endian host this produces the spec-correct little-endian output
that the tests assert; on a big-endian host it does not.

This matters because `infra::Endian.hpp` defines `isBigEndian` and genuinely branches on it, so
big-endian is a configuration the library treats as supported.

It is worth recording what the tests do and do not catch here. They assert literal expected
bytes for the scalar fields — manufacturer code 0x1234 becoming `0x34, 0x12`, appearance 0x03C1
becoming `0xC1, 0x03` — so a big-endian build would **fail loudly** rather than ship silently.
That is a real mitigation and the reason this is not rated more severely.

The 128-bit case is weaker. `AttAttribute::Uuid128` is `infra::BigEndian<std::array<uint8_t, 16>>`
(`Att.hpp:14`), and `SwapEndian` for arrays reverses them, so on a little-endian host the stored
bytes are the reverse of logical order — which is exactly the on-air little-endian order. The
formatter dumps that storage directly and is correct, but only by that coincidence: a type named
`BigEndian` is being used to obtain little-endian output. `TestGapAdvertisementFormatter.cpp:110-112`
then derives its expectation by reading the value back out and reversing it, rather than
asserting fixed bytes, so the test would follow the implementation if `BigEndian`'s semantics
ever changed.

### A Bluetooth device address is not a MAC address

The module builds on `hal::MacAddress`. The specification calls this a Bluetooth Device Address,
and its meaning is inseparable from its address type — the same six bytes mean different things
depending on whether they are public, static random, or a resolvable private address.

`GapAddress` (`GapTypes.hpp:77`) bundles address and type correctly, and then goes largely
unused: `GapCentral::Connect`, `GapCentral::SetAddress` and `GapBonding::IsDeviceBonded` all
take `(hal::MacAddress, GapDeviceAddressType)` as two loose parameters. Any call site can pair
an address with the wrong type, and the compiler cannot object. Threading `GapAddress` through
these signatures would make that mistake unrepresentable at no runtime cost.

### Citation discipline

Three files cite the specification. Every other magic number in the module — the AD types, the
advertising flags, the CCCD UUID and its values, all ten DIS UUIDs, the advertising interval
bounds — appears as a bare literal. All of those happen to be correct, but nothing records where
they came from or would flag it if one were edited to a wrong value.

No `.hpp` or `.cpp` names a Core Specification version. The only version named anywhere in the
module is 5.3, in `.proto` comments.

## Defects found along the way

Outside the conformance scope, but found while reading and worth fixing:

- `test_doubles/GattServerMock.hpp:22` declares `MOCK_METHOD(void, AddDescriptor, ...)` on
  `GattServerCharacteristicOperationsMock`, but `GattServerCharacteristicOperations` declares no
  such virtual function. **The mock method overrides nothing.**
- Most GAP mocks — `GapCentralMock`, `GapPeripheralMock`, `GapPairingMock`, `GapBondingMock`,
  `GattServerMock` — omit `(override)` on their `MOCK_METHOD` declarations, so a signature change
  in the interface would not fail the build. The GATT connection and client mocks do specify it.
- `BondStorageSynchronizerMock.hpp` still uses the legacy `MOCK_METHOD0`/`MOCK_METHOD1` macros
  while the rest of the directory has moved to modern `MOCK_METHOD`.
- `docs/Ble.md` states that the package provides "GAP, GATT and Direct Test Mode", but DTM is
  never described again and its C++ interface is in `hal/`, not here.

`docs/Ble.md` is otherwise accurate and unusually good — it explains *why* the GATT client is
per-connection, citing handle uniqueness per bearer, rather than merely stating that it is.

## Remediation backlog

Ordered by risk, not by effort.

| Priority | Items | Tracked as | Why first |
| --- | --- | --- | --- |
| **P1** | IoCapabilities C++/proto divergence (SM S1) | [#119](https://github.com/embedded-pro/embedded-infra-lib/issues/119) | Each can produce a wrong result on a correct-looking call. |
| **P1** | Supervision timeout unit and name (GAP S1) | [#120](https://github.com/embedded-pro/embedded-infra-lib/issues/120) | A tenfold unit error in a connection-critical parameter. |
| **P1** | AD codec endianness and alignment (cross-cutting S1) | [#121](https://github.com/embedded-pro/embedded-infra-lib/issues/121) | Can fault on ARMv6-M, a target this repo has a preset for. |
| **P2** | `GapConnectionParameters` naming; misfiled DLE constants | [#122](https://github.com/embedded-pro/embedded-infra-lib/issues/122) | Breaking changes, best sequenced with #120 in the same struct. |
| **P3** | `databaseOutOfSync` and cache coherence (GATT S3) | [#123](https://github.com/embedded-pro/embedded-infra-lib/issues/123) | Removes a capability outright rather than narrowing one. |
| **P3** | Long reads and writes (GATT S3) | [#124](https://github.com/embedded-pro/embedded-infra-lib/issues/124) | The 20-byte ceiling will be hit by the first port that needs it. |
| **P3** | Bond strength and security mode/level modelling (SM S3) | [#125](https://github.com/embedded-pro/embedded-infra-lib/issues/125) | Security-relevant: an app cannot tell a Just Works bond from an authenticated LESC one. |
| **P4** | Advertising asymmetry, AD types, scan parameters, extended advertising, PHY | [#126](https://github.com/embedded-pro/embedded-infra-lib/issues/126) | Coverage gaps that force callers around the abstraction. |
| **P4** | GATT declaration UUIDs and included services | [#127](https://github.com/embedded-pro/embedded-infra-lib/issues/127) | Every port will otherwise redefine them locally. |
| **P5** | L2CAP: non-goal or gap? | [#129](https://github.com/embedded-pro/embedded-infra-lib/issues/129) | Cheap to resolve either way; makes the boundary legible. |
| **P6** | Dead `valueHandleOffset`, miscited permission flags, mock defects, docs | [#128](https://github.com/embedded-pro/embedded-infra-lib/issues/128) | Cheap, low-risk, improves the next reader's odds. |


Several P1 and P2 items are breaking changes to a module that has already been rewritten twice
recently. Sequencing them is a maintainer decision this report is meant to inform, not pre-empt.

## Not assessed

Bluetooth Mesh, LE Audio and isochronous channels, direction finding and constant tone
extension, connection subrating, and the ECHO method-id discipline of the `.proto` files — the
last of which is already covered by `TestGapProto.cpp` and `TestGattProto.cpp`.

Whether any downstream vendor port has already been bitten by the `IoCapabilities` divergence
cannot be determined from this repository.
