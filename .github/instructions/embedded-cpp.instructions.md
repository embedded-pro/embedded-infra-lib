---
description: "Embedded C++ coding rules for EmIL: no heap allocation, bounded containers, event-driven non-blocking model, Allman brace style, PascalCase naming, SOLID principles, const correctness."
applyTo: "**/*.{hpp,cpp,h}"
---

# EmIL Embedded C++ Rules

This project is a heap-less, STL-like C++17 library for embedded microcontrollers. Follow these rules strictly.

## Memory — No Heap Allocation

Never use `new`, `delete`, `malloc`, `free`, `std::make_unique`, or `std::make_shared`.

Replace standard containers:
- `std::vector<T>` → `infra::BoundedVector<T>::WithMaxSize<N>`
- `std::string` → `infra::BoundedString::WithStorage<N>`
- `std::deque<T>` → `infra::BoundedDeque<T>::WithMaxSize<N>`
- `std::list<T>` → `infra::BoundedList<T>::WithMaxSize<N>` or `infra::IntrusiveList<T>`
- Use `std::array<T, N>` for fixed-size arrays
- Use `infra::Optional<T>` instead of pointer-as-optional

## Naming

- Classes/Methods: `PascalCase`
- Member variables/enum values: `camelCase`
- Namespaces: lowercase (`infra`, `hal`, `services`)
- Header guards: `MODULE_FOLDER_FILENAME_HPP`

## Style

- Allman braces (opening brace on new line), 4-space indent
- Prefer `{}` initialization over `()` for all variable and object initialization
- Functions ~30 lines max
- Self-documenting code — avoid unnecessary comments
- `const` on all non-mutating methods, `constexpr` where possible
- Fixed-size types: `uint8_t`, `int32_t`, etc.

## Execution Model

- Non-blocking, event-driven via `infra::EventDispatcher`
- Schedule callbacks with `infra::Function<void()>`
- Use `infra::WeakPtr<T>` for safe async scheduling
- No exceptions — use `infra::Optional` and error codes
- `really_assert()` for debug preconditions

## Design

- SOLID principles — constructor injection, depend on abstractions
- DRY — extract shared logic into helpers or templates
- RAII for resource management

Full details: [CodingStandard.md](../../docs/CodingStandard.md), [Containers.md](../../docs/Containers.md), [ExecutionModel.md](../../docs/ExecutionModel.md)
