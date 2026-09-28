# Hardware-in-the-loop terminal

## Introduction

`services/hil` exposes the peripherals of a board over an EMIL terminal, so that a host can validate a HAL on real hardware with a plain serial connection.
Every command answers with exactly one `OK` or `ERR` line and asynchronous notifications are `EVT` lines, which keeps the host side a simple line parser.

The package is split in two libraries:

- `services.hil` holds the protocol plumbing: `HilResponse`, `HilArguments`, `HilPinNaming`, `HilPinPool`, `HilTerminal`, `HilSystemCommands` and the `HilBind` helper.
- `services.hil.commands` holds one command group per peripheral, written against the `hal::` interfaces.
  Each group owns the lifecycle of its instance (index, `busy`/`notopen`, timeouts, releasing pins, `OK`/`EVT` formatting) and delegates opening and closing the driver to a factory that the HAL implements.

A HAL therefore only supplies board data, pin naming, a pin factory and one factory per peripheral it supports.

## Framing

- The host sends one command per line, terminated by `\r`. The terminal echoes characters and prints a prompt; the host ignores echo and prompt.
- Every command produces exactly one final line, either `OK` followed by optional `key=value` pairs, or `ERR <reason>` where `<reason>` is one token (`usage`, `pin`, `busy`, `notopen`, `unsupported`, `range`, `timeout`, `failed`).
- Asynchronous notifications are single lines starting with `EVT <peripheral>` followed by `key=value` pairs. They can appear at any time, including between a command and its final line.
- Lines produced outside the processing of a command line (deferred results such as `delay`, and every `EVT`) start with `\r\n`, so an empty line or a bare `>` prompt may precede them.
- Command lines are at most 255 characters (EMIL terminal buffer). Unknown commands, unknown keys and a wrong number of positional arguments return `ERR usage`.
- After reset the firmware prints `EVT boot board=<name> family=<family> sysclk=<hz> reset=<cause>` once; `<name>`, `<family>` and `<cause>` come from the HAL's `HilBoardInfo`.
- Numbers are decimal unless prefixed with `0x`. Binary payloads are hex strings without separators (`a55a0102`), and `-` stands for an empty payload where a command accepts one. Lists are comma separated without spaces.
- Pins are written as `P<port><index>`, for example `PF1`, `PJ0`, `PA15`; the HAL chooses the port letters and the highest index. Board aliases may be used wherever a pin is expected.
- Keys in arguments are `key=value`; positional arguments come first, in the order shown. Optional arguments are shown in brackets.
- Instance numbers are the hardware index; an index beyond what the factory reports returns `ERR range`.
- Opening an instance that is already open returns `ERR busy`; using one that is not open returns `ERR notopen`; `*.close` releases the driver and its pins so it can be reopened with different settings.
- Reserved pins (typically the terminal UART) cannot be claimed (`ERR busy`). A pin held by another open instance returns `ERR busy`; a pin the HAL does not offer for the requested function and instance returns `ERR pin`.
- Each group keeps one open instance at a time (the ADC group keeps a configurable number of sequencers, the GPIO group a configurable number of pins); one more returns `ERR busy`.

## General

- `ping` → `OK`
- `info` → `OK board=<name> family=<family> sysclk=<hz> reset=<cause> uid=<hex|none>`
- `reset` → no final line; the board resets after 20 ms and prints `EVT boot ...`
- `delay <ms>` → `OK` after the given time, at most 600000 ms; `ERR busy` while a delay is pending
- `board.pins` → `OK <alias>=<pin>,...` lists the aliases of the pin naming

## Command groups

The arguments of every `*.open` command (and of `wdt.start` beyond `timeout` and `feed`) are defined by the HAL's factory; the table lists what the generic group parses.

| Group                    | Commands and generic arguments                                                                                                                                                                                                                                                                                 |
|--------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `HilGpioCommands`        | `gpio.cfg <pin> <in\|out\|od> [pull=none\|up\|down] [drive=]`, `gpio.set <pin> <0\|1>`, `gpio.get <pin>` → `OK value=`, `gpio.pulse <pin> <count> <periodMs>`, `gpio.irq <pin> <rising\|falling\|both\|off> [type=immediate\|dispatched]`, `gpio.count <pin> [clear=0\|1]` → `OK count=`, `gpio.release <pin>` |
| `HilUartCommands`        | `uart.open <index> ...`, `uart.send <index> <hex>`, `uart.recv <index> [timeout=<ms>] [len=<n>]` → `OK data=`, `uart.close <index>`                                                                                                                                                                            |
| `HilSpiCommands`         | `spi.open <index> ...`, `spi.xfer <index> <txHex\|-> [rx=<n>] [continue=0\|1]` → `OK rx=`, `spi.close <index>`                                                                                                                                                                                                 |
| `HilAdcCommands`         | `adc.open <key>... ...`, `adc.measure <key>... [n=<runs>]` → `OK samples=<v>,...`, `adc.close <key>...`                                                                                                                                                                                                        |
| `HilCanCommands`         | `can.open <index> ...`, `can.send <index> <id> <hex\|-> [ext=0\|1]`, `can.close <index>`; `EVT can index= id= ext= data=` and `EVT can index= error=<name>`                                                                                                                                                    |
| `HilEepromCommands`      | `eeprom.write <address> <hex>`, `eeprom.read <address> <len>` → `OK data=`, `eeprom.erase`                                                                                                                                                                                                                     |
| `HilPwmCommands`         | `pwm.open <module> ...` → `OK` plus the factory's details, `pwm.duty <module> <duty%>...`, `pwm.freq <module> <hz>`, `pwm.stop <module>`, `pwm.close <module>`                                                                                                                                                 |
| `HilQeiCommands`         | `qei.open <index> ...`, `qei.read <index>` → `OK pos= dir=<fwd\|rev> speed= res=`, `qei.close <index>`                                                                                                                                                                                                         |
| `HilComparatorCommands`  | `comp.open <index> ...`, `comp.read <index>` → `OK out=`, `comp.irq <index> <rising\|falling\|both\|off>`, `comp.count <index> [clear=0\|1]` → `OK count=`, `comp.close <index>`                                                                                                                               |
| `HilWatchDogCommands`    | `wdt.start <index> timeout=<ms> [feed=auto\|manual] ...`, `wdt.feed <index>`; `EVT wdt index= warning=<n>`                                                                                                                                                                                                     |
| `HilEthernetCommands`    | `eth.open ...`, `eth.status` → `OK link=<up\|down> speed=<10\|100> duplex=<half\|full> rx= tx=`, `eth.close`                                                                                                                                                                                                   |
| `HilUnsupportedCommands` | answers `ERR unsupported` to every command name it is given                                                                                                                                                                                                                                                    |

Behaviour shared by the groups:

- `gpio.cfg` claims the pin with the alias pull when no `pull` is given; `od` starts released and takes no pull; `drive` is parsed by `HilPinFactory::ParseDrive`. `gpio.irq` returns `ERR unsupported` when `HilPinFactory::SupportsInterrupt` is false.
- `gpio.pulse` answers `OK` after `count` toggles, one every `periodMs`; `ERR usage` on an input.
- `uart.send` answers when the driver reports completion, or `ERR timeout` after 1 s plus the transmission time; `uart.recv` returns everything received since the last `uart.recv`, waiting up to `timeout` (default 1000, at most 10000) for `len` bytes when given.
- `spi.xfer` lasts max(tx, `rx`) bytes with the transmit data zero-padded and returns the first `rx` received bytes; `rx` defaults to the transmit length; `ERR timeout` after 1 s.
- `adc.measure` returns one value per sample of each run, at most the group's capacity; an asynchronous measurement returns `ERR timeout` after 1 s, and closing a measuring sequencer answers `ERR failed` before `OK`.
- `can.send` answers `OK` when the frame is sent, `ERR failed` when the driver reports failure and `ERR timeout` after 1 s; the same error repeated within 100 ms is reported once.
- `eeprom.*` always answer from the driver's completion, with `ERR timeout` after 5 s.
- `pwm.duty` takes one duty for all channels or one per channel, with up to 4 decimals (`12.5`).
- `wdt.start` needs `timeout` (1-30000); with `feed=auto` the group refreshes the watchdog on every early warning.

## Integrating a HAL

### Board, pins and terminal

- `HilBoardInfo` supplies `Name()`, `Family()`, `SystemClock()`, `ResetCause()` and `UniqueId()` (an empty range prints `uid=none`).
- `HilPinNamingDefault(portLetters, maximumIndex, aliases)` parses and prints pins. The port index is the position of the letter in `portLetters`; a character that is not a letter, such as `-`, skips a port. Tiva uses `"ABCDEFGHJKLMNPQ"` with index 0-7, STM32 `"ABCDEFGHIJK"` with index 0-15. Aliases carry a default `HilPull`.
- `HilPinFactory` answers `IsValid`, `SupportsFunction(pin, function, instance)`, `SupportsAnalog`, `SupportsInterrupt` and `ParseDrive`, and constructs or destroys the HAL's `hal::GpioPin` in a numbered slot. `function` is an opaque number the HAL maps to its own peripheral-function enumeration. `HilPinPool::WithCapacity<N>` uses slots `0` to `N - 1`, so the factory needs storage for `N` pins.
- `HilPinPool` shares a pin between analog users and gives exclusive use otherwise; reserved pins answer `ERR busy`.
- `HilTerminal` brackets every command line so that lines printed while processing it get no `\r\n` prefix, and answers `ERR usage` to unknown commands.

```cpp
static services::HilPinNamingDefault naming{ "ABCDEFGHJKLMNPQ", 7, infra::MakeRange(board::aliases) };
static TivaPinFactory pinFactory;
static services::HilPinPool::WithCapacity<32> pins{ pinFactory, infra::MakeRange(board::terminalPins) };
static services::HilResponse response{ tracer };
static services::HilTerminal::WithMaxQueueAndMaxHistory<256, 1> terminal{ uart, tracer, response };
static services::HilContext context{ response, pins, naming, terminal };

static TivaBoardInfo boardInfo;
static hal::cortex::Reset reset;
static services::HilSystemCommands system{ context, boardInfo, reset };
static services::HilGpioCommands::WithMaxPins<8> gpio{ context };
static TivaUartFactory uartFactory{ naming };
static services::HilUartCommands::WithCapacity<256, 112> uartCommands{ context, uartFactory };

system.PrintBoot();
```

### Factories

Every factory follows the same two-step open, so that argument errors are reported before `ERR busy` and resources are only claimed once the instance is free:

1. The group checks the shape against `OpenKeys()` and parses the instance number against `Instances()`.
2. `Prepare(index, arguments)` parses and validates the HAL-specific keys; it may also return `ERR busy` for instances the HAL reserves.
3. The group answers `ERR busy` if an instance is already open.
4. `Open(index, arguments, pins, ...)` claims pins through the `HilPinOwner` and constructs the driver. When it fails the group releases every pin claimed through the `HilPinOwner`.
5. `Close(index, onClosed)` destroys the driver and calls `onClosed`, immediately or later; a HAL that must quiesce the peripheral first (for example CAN, whose driver schedules events from its interrupt) masks the interrupt and destroys the driver from a scheduled event. The group reports `ERR notopen` for the instance until `onClosed` runs, then releases the pins and answers `OK`.

| Factory                | HAL-specific methods                                                                                                                                                                                      |
|------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `HilUartFactory`       | `Open(index, arguments, pins, hal::TimeKeeper&, HilUartHandle&)`; the handle holds a `hal::SerialCommunication*` and its baud rate, or a `hal::SynchronousSerialCommunication*` that uses the time keeper |
| `HilSpiFactory`        | `Open(index, arguments, pins, HilSpiHandle&)` with a `hal::SpiMaster*` or a `hal::SynchronousSpi*`                                                                                                        |
| `HilAdcFactory`        | `KeyPositionals()` and `ParseKey(arguments, key)` define the instance key (for example `<adc> <seq>`); `HilAdcHandle` holds a `hal::AdcMultiChannel*` or `hal::SynchronousAdc*` and the samples per run   |
| `HilCanFactory`        | `Open(index, arguments, pins, onError, hal::Can*&)`; the HAL reports driver errors by calling `onError` with the error name                                                                               |
| `HilEepromFactory`     | `Instance()` returns the `hal::Eeprom`, constructed on first use if the HAL wishes                                                                                                                        |
| `HilPwmFactory`        | `Open(module, arguments, pins, HilPwmHandle*&)`, `ReportOpened(module, line)` appends to the `OK` line, `ChangeFrequency(module, hz)` validates `pwm.freq`                                                |
| `HilQeiFactory`        | `Open(index, arguments, pins, hal::SynchronousQuadratureEncoder*&)`                                                                                                                                       |
| `HilComparatorFactory` | `Open(index, arguments, pins, HilComparatorHandle&)` with a `hal::AnalogComparator*` or `hal::SynchronousAnalogComparator*`                                                                               |
| `HilWatchDogFactory`   | `StartKeys()` lists every `wdt.start` key including `timeout` and `feed`; `Create(index, timeout, arguments, hal::Watchdog*&)`                                                                            |
| `HilEthernetFactory`   | `Open(arguments, HilEthernetHandle&)` with the `hal::EthernetSmi*` and `hal::EthernetMac*`; the group attaches a `HilEthernetMonitor`                                                                     |

`HilPwmAdapter<Driver>` turns any driver implementing one or more of `hal::SingleChannelPwm` ... `hal::FourChannelsPwm` (or their synchronous counterparts) into a `HilPwmHandle`; a single duty is replicated when the driver has no single-channel `Start`.
A HAL that rebuilds its PWM driver in place implements `HilPwmHandle` itself instead.

### Capacities

Buffers are provided through `WithStorage` aliases so a small target only pays for what it uses:

- `HilGpioCommands::WithMaxPins<N>`
- `HilUartCommands::WithCapacity<ReceiveBytes, TransmitBytes>`
- `HilSpiCommands::WithCapacity<Bytes>` (one transmit and one receive buffer)
- `HilAdcCommands::WithCapacity<Sequencers, Values>`
- `HilEepromCommands::WithCapacity<Bytes>`
- `HilEthernetCommands::WithReceiveBuffers<N>` (1536 bytes each)
- `HilUnsupportedCommands::WithMaxCommands<N>`

### Extending

- HAL-only commands are an extra `services::TerminalCommands` group built with `services::HilBind` and the shared `HilContext`; a group that needs the state of a generic group gets it from the factory it shares with that group (for example `pwm.fault` and `pwm.count` next to the HAL's `HilPwmFactory`).
- HAL-specific open options are simply more keys in `OpenKeys()`, parsed in `Prepare` or `Open` (for example ADC digital comparators).
- Extra groups claim pins with an owner from `HilOwners::extension` upwards.
- A peripheral the running MCU lacks is registered as a `HilUnsupportedCommands` group with its command names.

### Factory notes

- `Prepare` and `Open` both receive the same `HilArguments`; a factory that needs the parsed options in `Open` parses them again (a shared private `Parse` helper keeps it in one place).
- `Close` receives `onClosed` by reference; a factory that closes asynchronously (for example masking an interrupt and destroying the driver from a scheduled event so no queued event runs on a destroyed driver) copies it first.
- `HilAdcFactory::Open` and `Close` receive the group's slot index, so the HAL can keep its drivers in an array sized like `HilAdcCommands::WithCapacity`.
- Both libraries are ordinary static libraries; on small targets build them size-optimised (for example `-Os`) when the consumer's build type leaves optimisation off.
