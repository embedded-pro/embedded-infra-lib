# CLAUDE.md — embedded-infra-lib (EmIL)

## Project Overview

embedded-infra-lib (EmIL) is a C++20 library providing heap-less, STL-like infrastructure for embedded software development. It targets resource-constrained microcontrollers with strict memory and performance requirements, supporting Windows, Linux, macOS, and bare-metal ARM targets.

## Quick Start for Claude

For complex development tasks, use the specialized agents in `.claude/agents/`:

- **orchestrator** — Triage and route tasks; always start here for new work
- **planner** — Detailed implementation plans for complex features (uses Opus)
- **executor** — Implement code following all EmIL conventions
- **reviewer** — Structured code review against all EmIL standards

## Repository Structure

- **infra/** — Core infrastructure (containers, streams, syntax, timers, utilities)
  - `event/` — Event dispatching
  - `stream/` — Stream abstractions (input/output, bounded, limited)
  - `syntax/` — JSON, ProtoParser
  - `timer/` — Timers (system, periodic, single-shot)
  - `util/` — Bounded containers, Optional, Observer pattern, memory utilities
- **hal/** — Hardware Abstraction Layer
  - `interfaces/` — GPIO, I2C, SPI, Flash, UART, CAN abstractions
  - `generic/` — Generic HAL implementations
  - `unix/`, `windows/` — Platform-specific HAL
  - `synchronous_interfaces/` — Blocking HAL interfaces
- **drivers/** — Device drivers for specific hardware chips
  - `external_flash/` — SPI flash chip drivers (MicronN25q, CypressFll)
- **services/** — Higher-level services and protocols
  - `echo_core/` — ECHO RPC runtime
  - `network/` — Networking (connection, http, mqtt, dns, websocket, tls, sntp, etc.)
  - `network_instantiations/` — Platform-specific (BSD/Windows) network stack implementations
  - `ble/` — Bluetooth Low Energy
  - `sesame/` — SESAME secured serial protocol
  - `crypto/`, `flash/`, `tracer/`, `util/`
- **application/** — Standalone CLI tools and build-time executables
- **upgrade/** — Firmware upgrade and bootloader support
- **lwip/** — LwIP TCP/IP stack wrappers
- **osal/** — OS abstraction layer (FreeRTOS, ThreadX, std::thread)
- **external/** — Third-party dependencies (args, crypto, protobuf, Segger RTT)
- **cmake/** — CMake build modules and toolchain files
- **docs/** — Architecture documentation (consult before implementing)

## Critical Constraints

### Memory Management — ABSOLUTE RULES

**FORBIDDEN** — never use these in embedded-targeting code:
- `new`, `delete`, `malloc`, `free`
- `std::make_unique`, `std::make_shared`
- `std::vector`, `std::string`, `std::deque`, `std::list`, `std::map`, `std::set`

**REQUIRED** — use these instead:
- `infra::BoundedVector<T>::WithMaxSize<N>` instead of `std::vector<T>`
- `infra::BoundedString::WithStorage<N>` instead of `std::string`
- `infra::BoundedDeque<T>::WithMaxSize<N>` instead of `std::deque<T>`
- `infra::BoundedList<T>::WithMaxSize<N>` or `infra::IntrusiveList<T>` instead of `std::list<T>`
- `infra::Optional<T>` instead of pointer-as-optional or `std::optional<T>`
- `std::array<T, N>` for fixed-size arrays
- Stack or static allocation only

> **Exception**: `services/network_instantiations/`, `infra/stream/Std*`, and `infra/util/AllocatorHeap*` intentionally use heap-based STL types for host-platform (Linux/Windows) implementations only. These are not embedded targets.

### Execution Model — EVENT-DRIVEN, NON-BLOCKING

- Never block, sleep, or busy-wait
- Schedule async completions via `infra::EventDispatcher::Instance().Schedule()`
- Use `infra::Function<void()>` for callbacks (typically lambdas)
- Use `infra::WeakPtr<T>` when the scheduling object may be destroyed before the action executes — the action is automatically discarded if the object has expired
- Synchronous interfaces ONLY for bootloaders or contexts without an event dispatcher
- No mutexes/locks for state accessed only from the main event dispatcher

### Connection Lifetime Management

- `infra::SharedPtr<services::Connection>` for lifetime management
- Implement `services::ConnectionObserver` with `SendStreamAvailable()` and `DataReceived()`
- Request-based sending: call `RequestSendStream(size)`, write in `SendStreamAvailable()` callback; never write directly to a connection

## Design Principles

### SOLID
- **SRP**: Each class owns exactly one concern
- **OCP**: Extend via templates/compile-time polymorphism, not modification
- **LSP**: All derived classes fully substitutable
- **ISP**: Small, focused interfaces (e.g., `hal::GpioPin`, `hal::SpiMaster`)
- **DIP**: Constructor injection for dependencies; depend on abstractions

### DRY
Never duplicate logic. Use templates or helpers for shared code. Reuse existing infra components.

### Small Functions
~30 lines max (hard limit ~50). Extract named helpers. Each function does one thing.

## Coding Style

### Naming
- **Classes/Methods**: `PascalCase` — `Optional`, `InputStream`, `EnableInterrupt()`
- **Member variables**: `camelCase` — `initialized`, `storageAccess`
- **Enum values**: `camelCase` — `risingEdge`, `fallingEdge`
- **Namespaces**: lowercase — `infra`, `hal`, `services`, `drivers`, `application`
- **Header guards**: `MODULE_FOLDER_FILENAME_HPP`
- **Acronyms as words**: `Uart` not `UART`, `Spi` not `SPI` (except in `ALL_CAPS` macros)
- **No identifier prefixes**: no `s_`, `m_`, `_ptr`

### Brace Style — Allman, 4-space indent
```cpp
namespace services
{
    class MyComponent
    {
    public:
        void DoSomething();

    private:
        int value;
    };
}
```
Prefer `{}` initialization over `()` for all variable and object initialization.

### Interface Classes
- Pure virtual interfaces: no protected members, no constructor, no copy/move functions
- **Do NOT add `virtual ~ClassName() = 0`** — pure virtual destructors add significant vtable overhead in embedded systems

### Error Handling
- `infra::Optional<T>` for values that may not exist
- Return error codes or status enums — **NO EXCEPTIONS**
- `really_assert()` for debug-build precondition checks

### Comments
- **Avoid comments** — code should be self-documenting
- No `TODO`, `FIXME`, `HACK` in production code
- No docstrings unless API is non-obvious to a domain expert
- Acceptable: legal headers, `NOLINT` annotations, non-trivial domain clarifications

## Testing

- Test files: `{module}/test/Test{ComponentName}.cpp`
- Test doubles (mocks/stubs): `{module}/test_doubles/`
- Framework: GoogleTest + GoogleMock
- Use `testing::StrictMock<>` — **never `testing::NiceMock<>`**
- TDD: write tests before implementation (Red → Green → Refactor)
- No heap allocation in tests — same rules as production code
- Test pattern:
  ```cpp
  TEST(ComponentTest, specific_behavior_description)
  {
      // Arrange
      // Act
      // Assert
  }
  ```

## Build System

- CMake 3.24+, C++20 (`CMAKE_CXX_STANDARD 20`)
- Configure: `cmake --preset host`
- Build: `cmake --build --preset host-Debug`
- Test: `ctest --preset host`
- Coverage: `cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage`
- Cross-compile for ARM: `cmake --preset embedded`

### Key CMake Modules (in `cmake/`)
- `emil_test_helpers.cmake` — `emil_add_test()` function for test targets
- `emil_coverage.cmake` — Coverage instrumentation
- `emil_build_for.cmake` — Cross-compilation helpers
- `emil_clang_tools.cmake` — Linting and formatting

## ECHO and Protobuf Conventions

- `.proto` files must set `service_id` and `method_id` options (not names) for encoding
- All ECHO methods are async and must return `Nothing`
- ECHO methods with no input use the `Nothing` message type as the request
- Bound all unbounded fields with `(string_size)`, `(bytes_size)`, or `(array_size)` from `EchoAttributes.proto`
- Naming: MixedCase for files/messages/services/methods; camelCase for fields/enum values

## Version Control

- Atomic, focused commits following Conventional Commits
- Update CHANGELOG.md per release-please conventions

## Documentation Reference

Consult `docs/` before implementing in relevant areas:
- `docs/CodingStandard.md` — 61 coding rules (naming, spacing, braces)
- `docs/ExecutionModel.md` — Event dispatcher, async patterns, WeakPtr safety
- `docs/Containers.md` — BoundedVector, BoundedDeque, BoundedString, IntrusiveList
- `docs/MemoryRange.md` — ByteRange/ConstByteRange usage and helpers
- `docs/NetworkConnections.md` — Connection/ConnectionObserver, SharedPtr lifetime
- `docs/Echo.md` — ECHO RPC, protobuf encoding, service/method IDs
- `docs/Sesame.md` — SESAME serial protocol stack layers
