# embedded-infra-lib — Agent Rules

<!-- shared:begin 0.6.0 sha256:f57badde9903d1ff -->
Canonical rules for Claude, Copilot and sub-agents. Sections marked *(shared)* come from embedded-pro/embedded-agent-kit:
edit them there, not here. *(repo)* sections belong to this repository. *Exceptions* overrides a shared rule.

## Behaviour *(shared)*

- **Be as terse as possible.** No preamble, summary or narration. Report file paths and pass/fail.
- Everything you write — code, docs, agent files, PR text — uses the fewest words that stay unambiguous.
- Prose (docs, comments, agent files, commit and PR text) follows ASD-STE100 Simplified Technical English:
  one word, one meaning; active voice; one instruction per sentence; sentences ≤ 20 words (procedures) or ≤ 25 (descriptions).
- Ambiguous request: ask. Clear request: act.
- Do only what was asked. No unrelated refactors, no extra features.
- Batch reads, don't re-read, prefer targeted edits.
- Never change a shared section. Add a deviation to *Exceptions* only when the user approves it.

## Workflow *(shared)*

1. Multi-file or architectural change: update the design document first. Get approval before you write code.
2. Read *Domain* and *Build*. Read each `.github/instructions/*.instructions.md` whose `applyTo` matches a file you change.
3. Find the nearest existing pattern. Follow it.
4. Per behaviour: write a failing test, write the minimum code that passes it, refactor with all tests green.
5. Add each new file to its `CMakeLists.txt`.
6. Update the documentation for each behavioural change.
7. Run the *Build* commands until build and tests pass. Never disable, skip or weaken a test to get green.
8. Non-trivial change: run the `reviewer` agent, or ask the user to run it. Fix each CRITICAL and WARNING finding.
9. Report changed files, build pass/fail, test pass/fail.

## Memory *(shared)*

Scope: production code. Tests and host tools may use the heap.

- Forbidden: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`,
  `std::vector`/`string`/`deque`/`list`/`map`/`set`, recursion.
- Use: `infra::BoundedVector<T>::WithMaxSize<N>`, `infra::BoundedString::WithStorage<N>`,
  `infra::BoundedDeque`, `infra::BoundedList`/`IntrusiveList`, `std::array`, `std::optional`, `std::variant`.

## Interfaces & errors *(shared)*

- No virtual destructors: each adds a deleting destructor and thunks to flash.
  Interface = pure virtual methods + `protected: ~I() = default;` (non-virtual).
- Public `virtual ~Base() = default;` only on a base destroyed through a base pointer:
  `delete`/`std::unique_ptr<Base>`, `infra::PolymorphicVariant<Base, ...>`, `infra::OptionalForPolymorphicObjects<Base, ...>`.
  Put it on that base, not on every interface.
- Not needed for `std::variant`, `std::optional<Concrete>`, `infra::SharedPtr`: they destroy the concrete type.
- No exceptions. `std::optional`, `bool` or status enum; `really_assert` for preconditions. Never swallow an error.
- Constructor injection; no global state.

## Optimisation *(shared)*

- No `#pragma GCC optimize`, no `optimize` attribute: GCC won't inline across differing options.
  The consumer sets flags per translation unit.
- `OPTIMIZE_FOR_SPEED` only on hot functions defined in a header.
- Hot paths: no heap, no virtual dispatch, no blocking.

## Style *(shared)*

- `.clang-format` is authoritative. Allman, 4 spaces, `{}` init.
- PascalCase types/methods; camelCase members and enum values; lowercase namespaces.
  No `I` prefix; implementations end in `Impl`.
- `#pragma once` only.
- Fixed-width integers. `const`/`constexpr`-correct. Functions ≤ 30 lines (hard limit 50).
- Comments: non-obvious *why* only, one line. No `TODO`/`FIXME`, commented-out code, or restating the code.
  Fix or delete a comment the change makes stale.

## Testing *(shared)*

- `TEST_F` for fixtures, `TYPED_TEST` for types, plain `TEST()` for stateless checks.
- `StrictMock` only. `EXPECT_NEAR` with explicit tolerance. Fixture in an anonymous namespace, macros outside.
- Heap allowed in tests.

## Documentation *(shared)*

- Update docs before or with any behavioural change.
- One fact, one place: link, never restate.
- Visuals: Mermaid or ASCII.

## Desktop tools *(shared)*

Scope: host tools that people install and run, GUI or console.

- Open the log file first in `main`, before the application object.
  A GUI-subsystem build (`WIN32_EXECUTABLE`) has no stderr: the log file is its only output.
- Write the log to a per-user writable folder (`AppLocalDataLocation`) and rotate it.
  Capture the toolkit's messages (`qInstallMessageHandler`) from debug level.
- Install a crash handler after the log. It writes a `FATAL` line on a crash signal, `std::terminate`
  or an unhandled SEH exception, and a minidump on Windows. Qt tools use ui-cpp's `IssueLog` and `CrashReporter`.
- Log each start-up step: version, OS, toolkit version, arguments, restored settings, first frame.
- Provide `--safe-start`: it skips the saved state, so a bad saved state cannot block start-up.
- Give the log folder a menu item (Help ▸ Open Log Folder).
- Install test: start the tool, close it normally, then start it again with saved state.
  Print the log. On a failure, upload the log folder and the dumps.
<!-- shared:end -->

## Domain *(repo)*

- EmIL: a heap-less, STL-like C++20 library for embedded targets. It builds for Windows, Linux, macOS and bare-metal
  Cortex-M. It is a copy of [amp-embedded-infra-lib](https://github.com/philips-software/amp-embedded-infra-lib).
- Layout and namespace:
  - `infra/` (`infra`) — `event/`, `stream/`, `syntax/` (JSON, ProtoParser), `timer/`, `util/` (bounded containers,
    observers, `SharedPtr`).
  - `hal/` (`hal`) — `interfaces/`, `generic/`, `unix/`, `windows/`, `synchronous_interfaces/`.
  - `drivers/` (`drivers`) — chip drivers: audio, camera, display, eeprom, imu, microphones, motor_controller,
    touch_screen.
  - `services/` (`services`) — `echo_core/`, `network/`, `network_instantiations/`, `ble/`, `sesame/`, `fsm/`,
    `crypto/`, `flash/`, `tracer/`, `util/`.
  - `application/` (`application`) — host tools and build-time executables (ECHO console, protoc plugins,
    `bootloader_image`, key generators).
  - `upgrade/`, `bootloader/`, `lwip/`, `osal/` (FreeRTOS, ThreadX, `std::thread`), `external/`, `cmake/`.
- *Memory* does not cover host-only code: `services/network_instantiations/`, `infra/stream/Std*`,
  `infra/util/AllocatorHeap*`.
- Execution model (`docs/ExecutionModel.md`):
  - One `infra::EventDispatcher` runs actions in sequence. An action never blocks, sleeps or busy-waits.
  - Schedule a completion with `Schedule()` and an `infra::Function<void()>`; do not wait for it.
  - No locks for state that only the dispatcher touches.
  - An object that can die before its action runs schedules through `infra::EventDispatcherWithWeakPtr`
    with an `infra::WeakPtr<T>`.
  - Synchronous HAL interfaces only where no dispatcher exists (bootloaders). Threads only for isolation;
    they report back through `Schedule()`.
- Connections (`docs/NetworkConnections.md`): own with `infra::SharedPtr<services::Connection>`; observe with
  `services::ConnectionObserver` (`SendStreamAvailable()`, `DataReceived()`). Send only through
  `RequestSendStream(size)` and write in `SendStreamAvailable()`.
- Interfaces follow `hal/interfaces/Watchdog.hpp`: protected default constructor, deleted copy, protected
  non-virtual destructor.
- No `infra::Optional`: use `std::optional`. `infra::OptionalForPolymorphicObjects` is for a derived object in
  storage sized for its base.
- Ranges: build with `infra::MakeRange`, `infra::Head`, `infra::DiscardHead`, never from `std::array` iterators
  (they are class types on MSVC).
- ECHO and protobuf (`docs/Echo.md`):
  - Set `service_id` and `method_id` options; encoding uses them, not names.
  - Every ECHO method is asynchronous and returns `Nothing`; a method with no input takes `Nothing`.
  - Bound every unbounded field with `(string_size)`, `(bytes_size)` or `(array_size)` from `EchoAttributes.proto`.
  - MixedCase files, messages, services and methods; camelCase fields and enum values.
- Tests: `{module}/test/Test{Name}.cpp`, doubles in `{module}/test_doubles/`, registered with `emil_add_test()`.
- CMake modules in `cmake/`: `emil_test_helpers`, `emil_coverage`, `emil_build_for`, `emil_compiler_warnings`
  (`emil_enable_warnings()`), `emil_clang_tools`, `emil_native_executables`, `emil_bootloader_image`.
- Read the matching document in `docs/` before you change that area: `CodingStandard.md`, `ExecutionModel.md`,
  `Containers.md`, `MemoryRange.md`, `NetworkConnections.md`, `Echo.md`, `Sesame.md`, `Bootloader.md`, `Fsm.md`,
  `NvmRecordStore.md`.
- Commits follow Conventional Commits; release-please writes `CHANGELOG.md`.

## Build *(repo)*

```bash
cmake --preset host && cmake --build --preset host-Debug
ctest --preset host
cmake --build --preset host-Debug --target format-check
cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage
cmake --preset embedded
```

Presets: `host`, `host-single-Debug`, `host-single-MinSizeRel`, `host-no-mbedtls`, `coverage`, `host-ClangMsvc`,
`embedded`, `embedded-FreeRTOS`, `embedded-ThreadX`, `cortex-m0plus`, `cortex-m33`, `qemu-m3`, `qemu-m4`, `qemu-m4f`,
`qemu-m7`, `qemu-m7f`.

## Exceptions *(repo)*

- Include guards: existing headers keep theirs (`NAMESPACE_FILENAME_HPP`); new headers use `#pragma once`
  (`docs/CodingStandard.md` Rule 56). Converting would conflict with every merge from upstream amp-embedded-infra-lib.
