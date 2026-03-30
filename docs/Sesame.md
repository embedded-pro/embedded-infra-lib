# SESAME (SErial, Secured, Authenticated, Messaging Exchange)

## Introduction

SESAME is the name of the stacked protocol that is used in the Embedded Infrastructure Library to encode, authenticate, and protect data sent over a serial connection. The SESAME protocol builds on top of the ECHO protocol and encapsulates an ECHO stream in a secured serial communication protocol. It consists of three mandatory layers (COBS, Windowed, Secured), and two optional convenience layers.

## Layers

```
+-------------------------------+
| ECHO                          |
+-------------------------------+
|       (COBS-Windowed)         | <- Optional combined layer
+-------------------------------+
| COBS       | Windowed         | <- Can be used separately
+-------------------------------+
| (Optional Secured layer)      |
+-------------------------------+
| Physical layer (UART, ...)    |
+-------------------------------+
```

### COBS

This layer encodes the data using the Consistent Overhead Byte Stuffing (COBS) algorithm. COBS is a byte-oriented protocol that delimits frames with a `0x00` byte; that delimiter is removed from the body of the frame by an efficient encoding algorithm that adds at most one byte of overhead per 254 bytes. This encoding ensures that the `0x00` byte never appears in the body of the message, so it can be used to reliably detect frame boundaries.

The protocol encodes each frame as follows:

```
<encoded_data> <0x00>
```

where `encoded_data` is the COBS-encoded version of the original data, and `0x00` marks the end of the frame.

### Windowed

This layer adds flow control and basic reliability to data exchange. ECHO messages are typically small, so a simple window-based flow control is used, where the receiver indicates how much data it can accept.

The Windowed protocol wraps the data as follows:

```
<operation> [<data>]
```

Possible operations:

| Operation | Value | Description |
|---|---|---|
| `requestInit` | 0 | Request initialization of the connection |
| `acknowledgeInit` | 1 | Acknowledge that the connection is initialized |
| `requestInitWithData` | 2 | Request initialization with initial data to set window size |
| `acknowledgeInitWithData` | 3 | Acknowledge initialization with data |
| `releaseWindow` | 4 | Release window space, indicating the receiver can accept more data |
| `data` | 5 | Send a data frame |

A typical initialization exchange looks like this:

```
Initiator                  Responder
    |--- requestInit -------->|
    |<-- acknowledgeInit -----|
    |                         |
    |--- data --------------->|
    |<-- releaseWindow -------|
```

### Secured

This layer provides encryption and authentication using AES-CTR encryption with HMAC-SHA256 authentication. The secured layer encrypts data, adds a message authentication code, and manages key exchange.

The Secured protocol wraps the data as follows:

```
<nonce> <encrypted_data> <hmac>
```

where:

- `nonce` is a unique value used once with the encryption key to produce a unique key stream
- `encrypted_data` is the AES-CTR encrypted payload
- `hmac` is the HMAC-SHA256 authentication tag (truncated to configured length)

### Key Establishment

The Secured layer requires shared keys for encryption and authentication. Two key establishment schemes are supported:

#### Diffie-Hellman Key Exchange

Both peers perform an ephemeral Diffie-Hellman key exchange:

```
Initiator                          Responder
    |--- DH public key ------------>|
    |<-- DH public key -------------|
    |                               |
    | (both derive shared secret)   |
    |                               |
    |=== encrypted channel ========>|
```

The shared secret is derived using HKDF-SHA256 to produce separate keys for encryption and authentication.

#### Pre-Shared Key (PSK)

Both peers share a pre-configured key. This is simpler but requires secure key provisioning.

## Combining Layers

For convenience, the library provides combined implementations:

- **COBS-Windowed**: Combines COBS framing with Windowed flow control in a single layer for efficiency
- **Full SESAME stack**: Combines all three mandatory layers (COBS + Windowed + Secured) into a complete secured serial communication stack

## Integration with ECHO

SESAME provides the transport layer for ECHO communication over serial connections. The ECHO protocol handles the RPC encoding/decoding on top of the reliable, optionally secured, byte stream provided by SESAME.

A typical usage:

```
Application
    |
    v
ECHO (RPC encoding)
    |
    v
SESAME (COBS + Windowed + Secured)
    |
    v
UART (physical transport)
```
