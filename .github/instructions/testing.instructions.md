---
description: "EmIL testing guidelines: GoogleTest/GoogleMock patterns, StrictMock usage, test file naming, no heap allocation in tests, Arrange-Act-Assert pattern."
applyTo: "**/test/**"
---

# EmIL Testing Guidelines

## File Structure

- Test files: `{module}/test/Test{ComponentName}.cpp`
- Test doubles (mocks/stubs): `{module}/test_doubles/`
- CMake: tests added via `emil_add_test()` in `CMakeLists.txt`

## Framework

- GoogleTest for assertions (`TEST()`, `TEST_F()`)
- GoogleMock for mocking (`testing::StrictMock<>`)
- No heap allocation in tests — same rules as production code

## Test Pattern

```cpp
#include "module/path/Component.hpp"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

TEST(ComponentTest, specific_behavior_description)
{
    // Arrange
    ComponentUnderTest component(dependencies);

    // Act
    component.DoSomething();

    // Assert
    EXPECT_EQ(component.GetValue(), expected);
}
```

## Rules

- Use `testing::StrictMock<MockType>` for strict mock expectations
- Test edge cases and boundary conditions
- Test one behavior per test — keep tests focused
- Use descriptive test names that explain the scenario
- Allman brace style and PascalCase naming apply to test code too
- Use `really_assert()` death tests where appropriate
