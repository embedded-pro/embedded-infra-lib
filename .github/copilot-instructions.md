# GitHub Copilot Instructions for embedded-infra-lib (EmIL)

## Project Overview

embedded-infra-lib (EmIL) is a header-based C++17 library providing heap-less, STL-like infrastructure for embedded software development. It targets resource-constrained microcontrollers with strict memory and performance requirements, supporting Windows, Linux, macOS, and bare-metal ARM targets.

## Repository Structure

- **infra/**: Core infrastructure (containers, streams, syntax, timers, utilities)
  - `event/`: Event dispatching
  - `stream/`: Stream abstractions (input/output, bounded, limited)
  - `syntax/`: JSON, ProtoParser
  - `timer/`: Timers (system, periodic, single-shot)
  - `util/`: Bounded containers, Optional, Observer pattern, memory utilities
- **hal/**: Hardware Abstraction Layer
  - `interfaces/`: GPIO, I2C, SPI, Flash, UART, CAN abstractions
  - `generic/`: Generic HAL implementations
  - `unix/`, `windows/`: Platform-specific HAL
  - `synchronous_interfaces/`: Blocking HAL interfaces
- **services/**: Higher-level services (networking, BLE, crypto, echo, flash, tracing)
- **protobuf/**: Protocol buffer support with ECHO RPC framework
- **lwip/**: LwIP TCP/IP stack wrappers
- **osal/**: OS abstraction layer (FreeRTOS, ThreadX, std::thread)
- **upgrade/**: Firmware upgrade and bootloader support
- **external/**: Third-party dependencies (args, crypto, protobuf, Segger RTT)
- **cmake/**: CMake build modules and toolchain files
- **examples/**: Usage examples

## Critical Constraints

### Memory Management

- **NO HEAP ALLOCATION**: Never use `new`, `delete`, `malloc`, `free`, `std::make_unique`, or `std::make_shared`
- **NO DYNAMIC CONTAINERS**: Replace standard library containers that use dynamic allocation:
  - Use `infra::BoundedVector` instead of `std::vector`
  - Use `infra::BoundedString` instead of `std::string`
  - Use `infra::BoundedDeque` instead of `std::deque`
  - Use `infra::BoundedList` instead of `std::list`
  - Use `infra::IntrusiveList` for intrusive linked lists
- **STATIC ALLOCATION**: All memory must be allocated at compile-time or on the stack
- **AVOID RECURSION**: Stack usage must be predictable and minimal

### Performance Requirements

- **REAL-TIME CONSTRAINTS**: Code must execute deterministically within strict timing requirements
- **AVOID VIRTUAL CALLS IN ISR**: Virtual function calls add overhead; avoid in interrupt service routines
- **INLINE CRITICAL CODE**: Use `inline` for small, frequently-called functions
- **CONST CORRECTNESS**: Mark all non-mutating methods as `const` for compiler optimization
- **PREFER CONSTEXPR**: Use `constexpr` for compile-time calculations when possible

### Memory Consumption

- **MINIMIZE FOOTPRINT**: Every byte counts in embedded systems
- **USE FIXED-SIZE TYPES**: Prefer `uint8_t`, `int32_t`, etc., over `int` for predictable sizing
- **PACK STRUCTURES**: Use pragma pack or compiler attributes when appropriate
- **AVOID COPYING**: Use references and move semantics to prevent unnecessary copies
- **MEASURE SIZE**: Be aware of sizeof() for all data structures

### Execution Model

- **EVENT-DRIVEN, NON-BLOCKING**: The primary execution model uses an event dispatcher (`infra::EventDispatcher`). Actions are scheduled via `Schedule()` with `infra::Function<void()>` (typically lambdas) and executed one after another on the main thread — no action may block, sleep, or busy-wait
- **NO SYNCHRONIZATION NEEDED**: Since queued actions run sequentially on a single event dispatcher, no mutexes or locks are required for shared state accessed only from that dispatcher
- **SCHEDULE COMPLETION, DON'T WAIT**: When starting an asynchronous operation (e.g. flash write, network request), schedule a new action upon completion instead of waiting for the result. This keeps the dispatcher responsive and allows other components to make progress
- **WEAK POINTER SAFETY**: Use `infra::EventDispatcherWithWeakPtr` and schedule with an `infra::WeakPtr<T>` when the scheduling object may be destroyed before the action executes — the action is automatically discarded if the object has expired
- **SYNCHRONOUS INTERFACES ARE THE EXCEPTION**: Synchronous (blocking) HAL interfaces are reserved for constrained contexts (e.g. boot loaders or applications that run without an event dispatcher). Default to asynchronous interfaces whenever an event dispatcher is present
- **MULTI-THREADING IS OPT-IN**: Multiple threads are only used when real-time guarantees or long-running computations require isolation from the main event dispatcher. Each thread may have its own event dispatcher; completion is reported back to the main dispatcher via `Schedule()`

### Connection Lifetime Management

- **SharedPtr OWNERSHIP**: Network connections use `infra::SharedPtr<services::Connection>` for lifetime management — the connection lives as long as the shared pointer is held
- **Observer PATTERN**: `services::ConnectionObserver` attaches to a `Connection` via `SharedOwnedObserver`/`SharedOwningSubject` — implement `SendStreamAvailable()` and `DataReceived()`; override `Attached()`/`Detaching()` only when setup/cleanup logic is needed
- **REQUEST-BASED SENDING**: Never write directly to a connection; call `RequestSendStream(size)` and write in the `SendStreamAvailable()` callback

## Design Principles

These are the most important architectural directives in this codebase. Every new class, function, and CMake target must follow them.

### SOLID Principles

- **Single Responsibility (SRP)**: Each class owns exactly one concern. A stream class handles streaming, not parsing. An observer class handles notifications, not business logic. Each source file should address a single topic.
- **Open/Closed (OCP)**: Prefer templates and compile-time polymorphism for extending functionality without modifying existing code. Use the Observer pattern to extend behavior without changing subjects.
- **Liskov Substitution (LSP)**: When using abstract base classes (e.g., HAL interfaces like `hal::GpioPin`, `hal::SpiMaster`), all derived classes must be fully substitutable. Override every pure virtual method; do not weaken preconditions or strengthen postconditions.
- **Interface Segregation (ISP)**: Keep interfaces small and focused. HAL interfaces are split by concern (`hal::GpioPin`, `hal::AnalogPin`, `hal::SpiMaster`) — do not combine unrelated capabilities into one interface.
- **Dependency Inversion (DIP)**: High-level modules depend on abstractions, not concrete implementations. Use constructor injection for dependencies. Services depend on HAL interfaces, not platform-specific implementations.

### DRY (Don't Repeat Yourself)

- **NEVER duplicate logic**. If two functions share more than a few lines of identical code, extract a common helper, template, or base class.
- Use templates to eliminate per-type or per-size code duplication.
- Reuse existing infrastructure components (`infra::BoundedVector`, `infra::Observer`, `infra::Optional`) rather than reimplementing equivalents.
- Share configuration structs across layers instead of re-declaring equivalent types.

### Small Functions

- **Every function should do one thing and fit on one screen** (~30 lines max, hard limit ~50 lines).
- Extract named helpers from long methods — the name acts as self-documentation.
- Prefer returning values over mutating shared state — this makes functions easier to test and reason about.
- Complex state machines should delegate to per-state handler methods instead of large switch blocks.

## Coding Style and Patterns

### Naming Conventions

- **Classes**: PascalCase — `Optional`, `InputStream`, `GpioPin`, `BoundedVector`
- **Methods**: PascalCase — `Get()`, `Set()`, `Emplace()`, `EnableInterrupt()`
- **Member variables**: camelCase — `initialized`, `storageAccess`
- **Enum values**: camelCase — `risingEdge`, `fallingEdge`, `triState`
- **Namespaces**: lowercase — `infra`, `hal`, `services`
- **Header guards**: `MODULE_FOLDER_FILENAME_HPP` (e.g., `INFRA_UTIL_OPTIONAL_HPP`)

### Brace Style (Allman)

Opening braces on new lines for classes, namespaces, and functions. 4-space indentation:

```cpp
namespace infra
{
    class Example
    {
    public:
        void DoSomething();

    private:
        int value;
    };
}
```

### Dependency Injection

- Use constructor injection for dependencies
- Pass interfaces (abstract base classes), not concrete implementations
- Example:
  ```cpp
  namespace services
  {
      class FlashWriter
      {
      public:
          FlashWriter(hal::Flash& flash, infra::ByteRange buffer)
              : flash(flash)
              , buffer(buffer)
          {}

      private:
          hal::Flash& flash;
          infra::ByteRange buffer;
      };
  }
  ```

### Comments

- **AVOID COMMENTS**: Code should be self-documenting through clear naming, small functions, and expressive types
- Do not add comments that restate what the code does
- Do not add `TODO`, `FIXME`, or `HACK` comments in production code
- Do not add function/method docstrings unless the API is non-obvious to a domain expert
- Acceptable exceptions: legal headers, `NOLINT` annotations, and brief clarifications of non-trivial domain-specific logic

### Error Handling

- Use `infra::Optional` for functions that may not return a value
- Return error codes or status enums, not exceptions (no exceptions in this codebase)
- Assert preconditions in debug builds with `really_assert()`

### Observer Pattern

Use the built-in Observer/Subject pattern for event-driven communication:

```cpp
class SensorObserver : public hal::GpioPin::Observer
{
public:
    // ...
};
```

### Testing

- Write unit tests using GoogleTest and GoogleMock
- Test files in `{module}/test/Test{ComponentName}.cpp`
- Mock objects in `{module}/test_doubles/` directories
- Use `testing::StrictMock<>` for strict expectations
- Aim for high code coverage
- Test edge cases and boundary conditions
- Test pattern:
  ```cpp
  #include "infra/util/Optional.hpp"
  #include "gtest/gtest.h"

  TEST(OptionalTest, ConstructedEmpty)
  {
      infra::Optional<bool> o;
      EXPECT_FALSE(o);
  }
  ```

## Common Patterns

### Instead of this (BAD):
```cpp
std::vector<float> samples;
samples.push_back(value);

std::string message = "Error: " + errorCode;

auto ptr = std::make_unique<Driver>();

void* buffer = malloc(256);
```

### Do this (GOOD):
```cpp
infra::BoundedVector<float>::WithMaxSize<100> samples;
samples.push_back(value);

infra::BoundedString::WithStorage<64> message;
message = "Error: ";

// Stack allocation, no heap
Driver driver(dependencies);

std::array<uint8_t, 256> buffer;
```

## Additional Guidelines

- **RAII**: Use Resource Acquisition Is Initialization for resource management
- **INTERFACES**: Define interfaces (pure virtual classes) for testability and flexibility
- **NAMESPACES**: Use the appropriate namespace for the module:
  - `infra` — Core utilities, containers, streams, timers
  - `hal` — Hardware abstraction layer
  - `services` — Higher-level services and protocols
- **SELF-DOCUMENTING CODE**: Write clear, self-explanatory code with descriptive names
- **CODE FORMATTING**: Strictly adhere to the rules defined in `.clang-format` for consistent code style
- **TEMPLATE USAGE**: Use templates for type-generic components while maintaining type safety
- **CONSTEXPR CALCULATIONS**: Maximize compile-time computation for constants and lookup tables
- **USE FIXED-SIZE TYPES**: Prefer `uint8_t`, `int32_t`, etc., over `int` for predictable sizing
- **AVOID COPYING**: Use references and move semantics to prevent unnecessary copies

## Build System

- CMake-based build (minimum 3.24) with presets for different targets
- C++17 required (`CMAKE_CXX_STANDARD 17`)
- Support for host (simulation and testing) and embedded ARM targets
- Separate build configurations for Debug, Release, RelWithDebInfo, MinSizeRel
- GoogleTest for unit testing, fetched via `FetchContent`
- Coverage builds via the `coverage` CMake preset (`EMIL_ENABLE_COVERAGE=On`)
- Custom CMake modules in `cmake/` directory:
  - `emil_test_helpers.cmake` — Test setup and `emil_add_test()` function
  - `emil_coverage.cmake` — Coverage instrumentation
  - `emil_build_for.cmake` — Cross-compilation helpers
  - `emil_clang_tools.cmake` — Linting and formatting

### Cross-Platform Considerations

- Code must work on host (x86/x64) and target (ARM Cortex-M) platforms
- Use conditional compilation for platform-specific code (Windows, Linux, Darwin, Generic)
- HAL implementations are split per platform (`hal/unix/`, `hal/windows/`)
- Test on host before deploying to target hardware

### ECHO and Protobuf Conventions

- ECHO services use `service_id` and `method_id` options instead of names for encoding — always assign these in `.proto` files
- All ECHO methods are asynchronous and must return `Nothing`
- ECHO methods with no input must use the `Nothing` message type as their request
- Bound all unbounded Protobuf fields with `(string_size)`, `(bytes_size)`, or `(array_size)` options from `EchoAttributes.proto`
- Follow the proto style guide: MixedCase for files/messages/services/methods, camelCase for fields/enum values

## Version Control

- Keep commits atomic and focused
- Write clear commit messages following conventional commits
- Update CHANGELOG.md according to release-please conventions

## Documentation Reference

Detailed documentation is available in the `docs/` folder. Consult these when reviewing or writing code in the relevant areas:

- `docs/ExecutionModel.md` — Event dispatcher architecture, async patterns, WeakPtr safety, multi-threading guidelines
- `docs/MemoryRange.md` — `ByteRange`/`ConstByteRange` usage, range accessors, helper functions
- `docs/Containers.md` — `BoundedVector`, `BoundedDeque`, `BoundedString`, `IntrusiveList`, `WithMaxSize` pattern
- `docs/Echo.md` — ECHO RPC protocol, Protobuf message encoding, `EchoAttributes.proto`, service/method ID conventions
- `docs/Sesame.md` — SESAME serial protocol stack (COBS, Windowed, Secured layers), key establishment
- `docs/NetworkConnections.md` — `Connection`/`ConnectionObserver` pattern, `SharedPtr` lifetime management
- `docs/CodingStandard.md` — Complete C++ coding standard with 61 rules covering naming, spacing, braces, and more
