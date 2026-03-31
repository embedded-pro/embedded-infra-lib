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
- **drivers/**: Device drivers for specific hardware chips
  - `external_flash/`: SPI flash chip drivers (MicronN25q, CypressFll)
- **services/**: Higher-level services and protocols
  - `echo_core/`: ECHO RPC runtime (serialization, message send/receive)
  - `echo_attributes/`: ECHO protobuf attribute definitions
  - `echo/`: ECHO integration (on message communication, on sesame)
  - `network/`: Networking (split into sub-targets):
    - `connection/`: Base connection abstractions, address, name resolution
    - `http/`: HTTP client and server
    - `mqtt/`: MQTT client
    - `dns/`: DNS, mDNS, LLMNR, Bonjour
    - `websocket/`: WebSocket client and server
    - `tls/`: TLS/MbedTLS connections and certificates
    - `echo/`: ECHO-over-network (EchoOnConnection, proto generation)
    - `sntp/`: SNTP client
    - `ssdp/`: SSDP device discovery
    - `serial/`: Serial server
  - `network_instantiations/`: Ready-to-use network stack instantiations
  - `ble/`: Bluetooth Low Energy
  - `sesame/`: SESAME secured serial protocol
  - `crypto/`: Cryptographic services
  - `flash/`: Flash abstractions (QuadSpi, SPI, regions)
  - `tracer/`: Tracing services
  - `util/`: Service utilities
- **application/**: Standalone CLI tools and build-time executables
  - `echo_console/`: Interactive ECHO console application
  - `sesame_key_generator/`: SESAME key generation tool
  - `security_key_generator/`: Upgrade security key generator
  - `protoc_echo_plugin/`: Protobuf compiler ECHO plugin (C++)
  - `protoc_echo_plugin_csharp/`: Protobuf compiler ECHO plugin (C#)
  - `protoc_echo_plugin_java/`: Protobuf compiler ECHO plugin (Java)
- **upgrade/**: Firmware upgrade and bootloader support
- **lwip/**: LwIP TCP/IP stack wrappers
- **osal/**: OS abstraction layer (FreeRTOS, ThreadX, std::thread)
- **external/**: Third-party dependencies (args, crypto, protobuf, Segger RTT)
- **cmake/**: CMake build modules, toolchain files, and protocol_buffer_echo.cmake
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
  - `drivers` — Device drivers for specific hardware chips
  - `application` — Standalone CLI tools and build-time executables
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

## Version Control

- Keep commits atomic and focused
- Write clear commit messages following conventional commits
- Update CHANGELOG.md according to release-please conventions
