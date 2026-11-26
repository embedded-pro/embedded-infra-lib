# NVM Record Store

`services::NvmRecordStore` keeps one record across a reset. It stores two copies of the record in two
`services::NvmRegion`s, so a reset during a save tears one copy at most. The store does not know the
record contents: a client encodes and decodes the payload.

## Copies

Each copy is a header followed by the payload.

| Field    | Size    | Notes                                                            |
|----------|---------|------------------------------------------------------------------|
| Magic    | 2 bytes | Marks a written copy; an erased region reads 0xFFFF              |
| Sequence | 2 bytes | Increments on each save; selects the newer copy                  |
| Length   | 2 bytes | Payload size in bytes                                            |
| CRC      | 2 bytes | CRC-16/CCITT-FALSE over the first 6 header bytes and the payload |
| Payload  | Length  | Written by the client                                            |

All fields are little-endian. A copy is valid when the magic matches, the length fits the region, and
the CRC matches. Sequence numbers wrap; the newer of two valid copies is the one ahead by less than
half the range.

## Restore

The constructor claims access, reads both copies and releases access. When a valid copy exists, the
store gives the payload of the newest one to `Client::Restored`.

| `RestoreResult` | Meaning                                   |
|-----------------|-------------------------------------------|
| `pending`       | The copies are not read yet               |
| `restored`      | A valid copy was given to the client      |
| `defaults`      | Both regions are erased                   |
| `corrupt`       | No valid copy, and a region is not erased |

## Save

`ScheduleSave()` starts or restarts the save delay (`Config::saveDelay`, 2 s by default). A burst of
changes thus makes one write. When the delay expires:

1. When the restore is pending or `Client::Busy()` is true, the delay starts again.
2. The store claims access. Once granted, it checks `Busy()` again, because another user can hold
   the claim for a long time.
3. `Client::Capture` writes the payload and returns its size.
4. The store writes the copy that is not the newest valid one, then reads it back and compares.
5. The store releases access and records the result.

| `SaveResult` | Meaning                                  |
|--------------|------------------------------------------|
| `none`       | No save was requested                    |
| `pending`    | A save waits for the delay or for access |
| `saved`      | The read-back matches                    |
| `failed`     | The read-back differs                    |

A failed save is not retried. The next `ScheduleSave()` writes the same copy again. A
`ScheduleSave()` during a write starts a new save after the write completes.

## Shared access

A driver such as `drivers::Eeprom24xx` runs one operation at a time. Stores on one device share one
`infra::ClaimableResource`. A store without that argument owns its own resource.

```cpp
infra::ClaimableResource eepromAccess;
services::NvmEepromRegion tuningFirst{ eeprom, 0, 128 };
services::NvmEepromRegion tuningSecond{ eeprom, 128, 128 };
services::NvmRecordStore::WithRegionSize<128> tuning{ tuningFirst, tuningSecond, eepromAccess, tuningClient };
```

`WithRegionSize<N>` holds two buffers of N bytes: one to write and one to read back. A payload larger
than N minus the header size stops the program with an assertion.
