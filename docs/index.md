# embedded-infra-lib

## Introduction

embedded-infra-lib is a set of C++ libraries and headers that provide heap-less, [STL](https://en.wikipedia.org/wiki/Standard_Template_Library) like, infrastructure for embedded software development. It includes, amongst others; a hardware abstraction layer (HAL), a [remote procedure call](Echo.md) (RPC) implementation for TCP/IP and [Serial communication](Sesame.md), a [networking layer](NetworkConnections.md), a secure upgrade mechanism and several other re-usable utility classes.

## Overview

```
+---------------------------------------------------------+
| application                                             |
|   echo_console, protoc_echo_plugin, ...                 |
+---------------------------------------------------------+
| upgrade                                                 |
|   bootloader, pack_builder, ...                         |
+---------------------------------------------------------+
| services                                                |
|   echo_core, echo, network/{http,mqtt,dns,...}, ...     |
+---------------------------------------------------------+
| drivers                                                 |
|   external_flash                                        |
+---------------------------------------------------------+
| hal                                                     |
|   interfaces, synchronous_interfaces, ...               |
+---------------------------------------------------------+
| infra                                                   |
|   event, stream, syntax, timer, util                    |
+---------------------------------------------------------+
```

## infra

The `infra` package contains the building blocks that all further code is built upon. There are basics concept like [`infra::MemoryRange`](MemoryRange.md) that provides an abstraction for a block of memory and several [Containers](Containers.md) that provide fixed-size alternatives for the well-known standard library containers like: `std::vector`, `std::list`, `std::queue`, etc.

In the next few chapters the contents of the components within `infra` will be described.

### util

#### infra::BoundedString

`infra::BoundedString` is similar to `std::string` except that it can contain a maximum number of characters, and is not zero-terminated by default (see GitHub issue [#37](https://github.com/embedded-pro/embeddedinfralib/issues/37)).

```cpp
infra::BoundedString::WithStorage<5> string("abc");
EXPECT_EQ('a', string.front());
EXPECT_EQ('c', string.back());
EXPECT_FALSE(string.empty());
EXPECT_FALSE(string.full());
EXPECT_EQ(3, string.size());
EXPECT_EQ(5, string.max_size());
```

#### infra::CyclicBuffer

`infra::CyclicBuffer` transforms a given chunk of memory into a cyclic buffer. Data can be pushed into this buffer, and popped out again. With `ContiguousRange()`, the largest block starting at the start can be obtained. This is useful when feeding large blocks to e.g. DMA. In practice, the typedef `CyclicByteBuffer` will most often be used.

```cpp
infra::CyclicBuffer<uint8_t>::WithStorage<4> buffer;
buffer.Push(std::vector<uint8_t>{ 3, 7, 9 });
buffer.Pop(2);
buffer.Push(std::vector<uint8_t>{ 2, 4 });

EXPECT_EQ((std::vector<uint8_t>{ 9, 2 }), buffer.ContiguousRange());
```
