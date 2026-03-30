# Network Connections

## Introduction

A network connection is the result of an `infra::SharedPtr` to a `services::Connection`, and a `services::ConnectionObserver` that is attached to it. The `Connection` abstracts the details of the underlying TCP connection, while the `ConnectionObserver` handles the application-level protocol. The `Connection` provides methods to send and receive data, and notifies the `ConnectionObserver` of received data and connection status changes.

## Connection

The `Connection` class is the base interface for all network connections. It provides methods to request a send stream, to close the connection, and to get information about the connection. `Connection` inherits from `infra::SharedOwnedObserver`, which together with `infra::SharedOwningSubject` controls the lifetime of the connection and the observer. The connection is kept alive as long as the `infra::SharedPtr<Connection>` is held, and it is released when the `SharedPtr` goes out of scope.

The most important methods of `Connection`:

| Method | Description |
|---|---|
| `RequestSendStream(std::size_t sendSize)` | Request a stream to send data |
| `MaxSendStreamSize()` | Returns the maximum size of a send stream |
| `ReceiveStream()` | Returns the stream of received data |
| `AckReceived()` | Acknowledge that received data has been processed |
| `CloseAndDestroy()` | Gracefully close the connection |
| `AbortAndDestroy()` | Abort the connection immediately |

## ConnectionObserver

`ConnectionObserver` is the base class for handling events on a connection. It inherits from `infra::SharedOwningSubject`, so that the `ConnectionObserver` shares ownership over the `Connection`.

The most important methods of `ConnectionObserver`:

| Method | Description |
|---|---|
| `SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer)` | Called when a send stream becomes available |
| `DataReceived()` | Called when new data has been received |
| `Attached()` | Called when the observer is attached to a connection |
| `Detaching()` | Called when the observer is being detached |
| `Close()` | Called when the connection is being closed |

## Lifetime Management

Connections and observers use `infra::SharedPtr` and the `SharedOwnedObserver`/`SharedOwningSubject` pattern to manage lifetimes:

- The **Connection** is the `SharedOwnedObserver` — it is owned by whoever holds the `SharedPtr<Connection>`
- The **ConnectionObserver** is the `SharedOwningSubject` — it shares ownership over the `Connection` through the observer pattern

When a `ConnectionObserver` is attached to a `Connection`, the observer is allocated and bound to that connection. When the connection is closed (via `CloseAndDestroy()` or `AbortAndDestroy()`), the observer is detached and both objects are released once all shared pointers go out of scope.
