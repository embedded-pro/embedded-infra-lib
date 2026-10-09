# Execution Model

## Execution by using an event dispatcher

Embedded Infrastructure Library supports various execution models, but the most important execution model is execution by scheduling blocks of work on an event dispatcher. This is a lightweight way of being able to perform multiple tasks in parallel, without needing multiple stacks or synchronization.

The event dispatcher is used by scheduling an action by using its `Schedule()` method. `Schedule()` takes a single argument of type `infra::Function<void()>`, and is therefore often used in combination with lambda functions. `Schedule()` does not execute its action immediately, but pushes it on a queue for later execution. The event dispatcher is typically constantly running on the application's main thread, and executes queued actions one after another. This way, a scheduled action is completely finished before the next action is started, so in contrast with running actions in different threads no synchronization is needed.

An action executed by an event dispatcher should never waste any time; no action should ever sleep, waiting for an external task, such as a write towards flash, to finish. Instead, after having started a write towards flash, a new action is scheduled when the write has been finished. In the meantime, other actions can execute. This creates a decoupling between various different pieces of business logic: while one piece is busy with writing to flash, another piece may be answering an HTTP request, or reading out a temperature sensor. No real-time behaviour is guaranteed on the event dispatcher (any real-time behaviour required is implemented outside of event dispatchers, by either interrupt handlers or by using threads), but multiple components can flawlessly share execution time, with each component making steady progress.

While it is possible to have multiple event dispatchers (when using multiple threads, a specific thread may have its own event dispatcher for dedicated tasks), there is one event dispatcher globally available via `infra::EventDispatcher::Instance()`. This event dispatcher is used for generic tasks; for example, the `ConfigurationStore` uses the `Flash` interface to read and write towards flash; the completion of a read or write action is dispatched on this globally available event dispatcher. A typical `main` function will therefore start with declaring an event dispatcher, and end with invoking that event dispatcher's `Run()` method.

### Support for infra::WeakPtr

Objects that are managed by shared pointers may schedule actions to be executed, but it may happen that that object is destroyed before that action is executed. In that case, that action should be discarded instead of being executed. `infra::EventDispatcherWithWeakPtr` exists to facilitate this usecase. Next to the typical `Schedule()` function that takes an action as parameter, it supports an overload that takes a second parameter: an `infra::WeakPtr<T>` towards the object that schedules the action. That parameter is converted to a shared pointer just before executing the action: if this conversion succeeds, the object is still alive, and the action is executed with that shared pointer as parameter. If the conversion fails, then the object was already expired, and execution of that action is skipped.

## Multi-threaded execution

While currently no component in Embedded Infrastructure Library requires the usage of multiple threads, it is perfectly viable to use an operating system to start more than one thread. A reason to use multiple threads include having a dedicated thread for executing tasks that require real-time behaviour. By separating execution of the event dispatcher and execution of time-critical tasks, enough execution time can be guaranteed for real-time behaviour.

Another reason for using multiple threads is when a certain action takes a considerable amount of time, for instance generating a certificate takes multiple seconds. Executing that action on the main event dispatcher would delay the execution of other actions; a solution to this could be to generate the certificate in its own thread, and its completion can be scheduled on the main event dispatcher.

A thread may have its own event dispatcher. This removes the need for starting and stopping a thread; when a thread-aware event dispatcher is idle, it will pause its thread, and it will wake up its thread when new work is scheduled.

## Execution without an event dispatcher

Some applications do not benefit from having an event dispatcher. A boot loader's main focus is to be small and execute one thing; it only loads an application and therefore has no need to execute multiple actions in parallel. The small overhead that an event dispatcher brings does not bring any benefits, and should therefore not be needed in a boot loader. For this kind of usecase, variations of the interfaces for interacting with peripherals exist which work synchronously; they do not need to schedule their completion on an event dispatcher, but they complete their activities before returning. While for other libraries this is often the default behaviour, for Embedded Infrastructure Library this is the exception.

A number of components in Embedded Infrastructure Library assume the presence of an event dispatcher. This includes any component that makes use of an asynchronous interface. Obviously, without an event dispatcher such components cannot be used.

## Idling in low power

When the event dispatcher runs out of work it calls `Idle()`. `infra::LowPowerEventDispatcher` forwards that call to an `infra::LowPowerStrategy`, which decides how the processor waits for the next interrupt.

On Cortex-M, `hal::cortex::LowPowerStrategyWithModes` masks interrupts, checks that the event dispatcher is still idle, and then asks a `hal::LowPowerMode` to enter `hal::PowerMode::sleep` or `hal::PowerMode::deepSleep`. The core wakes on any pending interrupt, even with interrupts masked, so work scheduled from an interrupt between the idle check and the wait is never missed.

Deep sleep is chosen only when both of these hold:

- No component holds the `infra::MainClockReference`. Peripherals that need the main clock while a transfer is in progress, such as `services::LowPowerSpiMaster` and `services::LowPowerSerialCommunication`, hold it for that time.
- No timer is pending. The system tick stops in deep sleep, so a pending timer would be delayed until some other interrupt wakes the core.

`hal::LowPowerMode` is implemented per vendor, because deep sleep requires vendor-specific clock configuration.

### Preparing for and resuming from deep sleep

Work around deep sleep falls in two groups.

Short, synchronous work that must happen right before and right after each deep sleep, such as switching pins to analog or notifying a radio, goes in a `hal::cortex::DeepSleepObserver` attached to the strategy. `EnteringDeepSleep()` and `LeftDeepSleep()` are called only around deep sleep, not around sleep, with interrupts masked.

Observers must not block, but they may schedule work on the event dispatcher. Vendor implementations restore the run-mode clocks before `LeftDeepSleep()` is called.

Asynchronous work, such as putting an external sensor or flash into its low-power state over SPI or I2C, cannot run with interrupts masked. It runs on the event dispatcher before deep sleep is allowed, coordinated by an `infra::SystemStateManager`.

Every component that has to prepare is a `infra::SystemStateParticipant`: it starts its preparation when a state is requested, and calls `ReachedState()` when it has finished. The manager requests the next state only after all participants reached the current one.

The application holds the `infra::MainClockReference` while running, and releases it in a final state, after all participants have prepared:

```cpp
struct StatePrepareForDeepSleep : infra::SystemState<StatePrepareForDeepSleep> {};
struct StateReadyForDeepSleep : infra::SystemState<StateReadyForDeepSleep> {};

class PowerManager
    : public infra::SystemStateParticipant
{
public:
    PowerManager(infra::SystemStateManager& manager, infra::MainClockReference& mainClock)
        : infra::SystemStateParticipant(manager)
        , mainClock(mainClock)
    {
        mainClock.Refere();
    }

protected:
    void RequestState(infra::SystemStateBase state) override
    {
        if (state == StateReadyForDeepSleep())
            mainClock.Release();

        ReachedState();
    }

private:
    infra::MainClockReference& mainClock;
};
```

Resuming mirrors this: the interrupt that woke the device schedules a run of the states that restore the participants, and the application takes the `MainClockReference` again.

## Supervising the event dispatcher with a watchdog

A stuck event dispatcher does not stop interrupts, so refreshing a hardware watchdog from an interrupt alone does not detect it. `services::EventDispatcherWatchdogWorker<Worker>` extends any event dispatcher worker with supervision by a `hal::Watchdog`:

- The worker counts a step when an action starts and when it finishes, so an odd count means an action is executing.
- The watchdog raises an early-warning interrupt every `EarlyWarningPeriod()`, and the worker refreshes it from there. If the dispatcher is idle, or its steps advanced since the previous early warning, it is making progress. Otherwise the same action is still executing, and the early warning counts as missed.
- Once the missed early warnings cover the expiration timeout, `onExpired` is called from the interrupt so it can record why the device resets. After that the watchdog is no longer refreshed, so it resets the device even when `onExpired` returns.

`services::EventDispatcherWithWatchdog`, `services::EventDispatcherWithWeakPtrAndWatchdog` and `services::LowPowerEventDispatcherWithWeakPtrAndWatchdog` are ready-made combinations. They take the watchdog, the expiration timeout and `onExpired` before the arguments of the dispatcher they extend:

```cpp
services::LowPowerEventDispatcherWithWeakPtrAndWatchdog::WithSize<50> eventDispatcher(watchdog, std::chrono::milliseconds(1500), onExpired, lowPowerStrategy);
```

No timer is involved, so an idle dispatcher can enter deep sleep while it is supervised. Code that has to keep interrupts disabled for longer than the early-warning period, such as a flash erase, calls `Refresh()` on the watchdog directly.

## Streaming audio

`hal::AudioOutput` and `hal::AudioInput` stream interleaved 16-bit samples to and from a digital audio peripheral such as I2S or SAI. The streams describe the data path. A codec chip that has its own control interface is configured over I2C or SPI by a driver, which implements `hal::AudioOutput` itself on top of the peripheral's implementation.
`drivers::CodecAudioOutput` holds the lifecycle that such drivers share: it starts the stream before the codec is brought up, stops it only after the codec has powered down, keeps the application's callbacks silent until the codec is audible, and applies a volume or mute change once no register sequence is running.
A codec driver derives from it and only provides the register sequences. `hal::AudioOutput` also carries the output level, described below.

Audio has a hard deadline, but the event dispatcher guarantees no real-time behaviour. The interfaces bridge this by letting the implementation own a buffer that is cycled by DMA or an interrupt:

- Once per period the implementation schedules a callback on the event dispatcher. `AudioOutput` offers an empty range to fill, `AudioInput` offers a range of captured samples. The range is valid only during the callback, and `AudioOutput` must be filled before the callback returns.
- The number of periods the implementation buffers, multiplied by the period duration, is the longest the event dispatcher may be busy before audio is lost. This is chosen by the implementation, not by the interface; a longer buffer trades latency for tolerance.
- When the event dispatcher was too slow, the implementation calls `onUnderrun` (output ran out of samples) or `onOverrun` (input overwrote samples that were not yet delivered). The stream keeps running.
- `Stop(onStopped)` may be called from within the callbacks. No callback of the stream is made after `Stop()` returns. `onStopped` is called from the event dispatcher once the stream, and the codec behind it, has stopped. Start the stream again, call `Stop()` again and destroy the implementation only after that.
- An implementation that has to bring up a codec may withhold `onSamplesRequired` until the output is audible. The stream keeps running meanwhile and the samples that would have been played are silence.

`AudioOutput` also controls the level of the output:

- `SetVolume(percent, onDone)` takes 0 to 100, where 0 is silence and 100 is full scale. `SetMuted(muted, onDone)` silences the output without changing the volume, so unmuting restores it.
- Both may be called before `Start()`, while running and after `Stop()`, and the values persist across `Start()` and `Stop()`.
- `onDone` is called from the event dispatcher once the codec has applied the value. While the output is not running the value is only stored, and `onDone` is called without waiting for the codec.
- Each of them allows one outstanding call: call it again only after its `onDone`. A caller that changes the level continuously, such as a slider, therefore sends the most recent value after each completion.
- An implementation without a level control, such as a bare I2S or SAI peripheral, ignores them or scales the samples in software, and still calls `onDone`.

`drivers::Cs43l22` is a codec driver on top of `drivers::CodecAudioOutput`. The CS43L22 has no analog-to-digital converter, so it implements only `hal::AudioOutput`.
Its configuration can route one of its analog inputs to the outputs next to the stream, which is called analog passthrough. That input is mixed inside the codec while the output is started and its samples are never delivered to software, so it is not a `hal::AudioInput`.
The driver controls the reset pin of the codec. It holds the codec in reset while it is not in use and pulses it at every start.
The stream has to supply the master clock, and `masterClockRatio` in the configuration is the ratio of that clock to the sample rate. The driver derives the master clock divider of the codec from it and rejects a ratio that the datasheet does not allow for the sample rate, and a sample rate outside 4 to 96 kHz. The speaker amplifiers do not support a master clock of 16.9344 or 18.432 MHz.
`SetVolume` sets the master volume, and the volume of the analog passthrough when it is configured. `SetMuted` mutes both, with the soft ramp of the codec, without powering the amplifiers down.
Stopping follows the power-down sequence of the datasheet: mute, wait for the ramp, disable the ramps, power down, wait, pull the reset pin low and only then stop the stream.

Because the implementation owns the buffer, placement in DMA-capable memory and cache maintenance remain a concern of the vendor implementation.

A digital microphone with a pulse-density modulated (PDM) output, such as the MP34DT05 or the MP45DT02, delivers 1-bit samples at its clock rate. A peripheral that captures them, such as I2S or SAI, can implement `hal::AudioInput` for these raw bits instead of PCM.

In such a stream a channel is a microphone, and a frame has one 16-bit word per channel. A word holds the next 16 clock cycles of its microphone, the first in the most significant bit. The sample rate is the clock frequency divided by 16, so the peripheral also generates the clock of the microphone. Periods, overrun and `Stop(onStopped)` are those of any `hal::AudioInput`.

`drivers::PdmMicrophone` is the `hal::AudioInput` that a consumer sees. It starts the raw stream and gives its words to a `drivers::PdmToPcm`, which turns them into PCM. `drivers::PdmToPcm` is only an interface; the decimation filter that implements it is not part of this library.
`drivers::Mp34dt05` and `drivers::Mp45dt02` are `PdmMicrophone`s that supply the clock range and the start-up time of their chip.

The decimation of the converter fixes the microphone clock, `clock = sampleRate * decimation`. The driver rejects a clock outside the range of the microphone, or one that is not a multiple of 16.

The microphone needs some time after its clock starts before its output is valid, so the driver discards the first samples of every stream. It counts them instead of using a timer.

A microphone such as the MP45DT02 has an `L/R` pin that is wired to ground or to the supply. It selects whether the microphone drives its data while the clock is low or while it is high, so two microphones with opposite settings can share one data line as two channels. The driver does not see this pin; the peripheral that implements the raw `hal::AudioInput` samples the matching clock phase.

## Writing to a display

`hal::Display` accepts a rectangle of pixels from a buffer that the caller owns. It describes only the pixel path. Resetting the controller, powering the panel and driving a backlight are separate concerns of the driver or the application.

- At most one write is in flight per display. Starting a second write before `onDone` was called is a programming error.
- The pixels must stay valid until `onDone` is called. The implementation does not copy them, so a buffer that is handed to DMA is not touched in between.
- `onDone` is scheduled on the event dispatcher. It is never called from within the call that started the write, so a completion that starts the next write cannot recurse.
- `Format()` states the layout of the pixel bytes that the display expects. `hal::Display` does not convert pixels, so a graphics library is configured with a matching format, for example little-endian RGB565 or byte-swapped RGB565.
- `Write()` takes a packed buffer. `WriteWithStride()` takes a buffer in which a row starts `strideInBytes` after the previous row, which is what a library that renders into a full-screen buffer needs to flush part of that buffer.

Completion is delivered by the dispatcher, so a caller must not wait for it by polling on the dispatcher thread. A graphics library that renders into one buffer and then busy-waits for the flush to finish blocks the dispatcher that has to deliver the completion. Such a library has to render into a second buffer while the first one is in flight, or return to the dispatcher between flushes.

## Driving a MIPI DSI panel

`hal::DsiHost` sends DCS and generic DSI packets to a panel and reads DCS responses back. A vendor implements it for its DSI peripheral, and `drivers/display/mipi_dsi` builds on it to drive any panel that is described by a table of commands.

- One operation (`WriteDcs`, `WriteGeneric` or `ReadDcs`) is in flight per host. The buffers stay valid until `onDone`, which is never called from within the call that started the operation, so a completion can start the next one.
- `MaxParametersSize()` bounds the parameters of one `WriteDcs` and the data of one `WriteGeneric`. It is at least 4, the size of a column or page address.
- The host chooses the packet type from the number of bytes: a DCS write of up to one parameter is a short packet and longer ones are long packets, and a generic write of up to two bytes is a short packet. Virtual channel, low-power or high-speed transmission, bus turn-around and the maximum return packet size of a read are the concern of the host.
- Writes complete without a result, like `hal::Display` and `hal::SpiMaster`. Only `ReadDcs` reports `Result::timeout` or `Result::failed`.
- `hal::DsiVideoStream` is a separate interface for hosts that stream a frame buffer themselves. `Start` is only valid while the stream is stopped and `Stop` only while it runs.

Two classes drive a panel, both derived from `drivers::MipiDsiPanelCore`, which owns reset, initialization, sleep, wake and brightness:

- `drivers::MipiDsiDisplay` is a `hal::Display` for command-mode panels. A write sets the column and page address and then sends the pixels as write memory start and write memory continue packets, each no larger than the host accepts and always a whole number of pixels.
  Rows of a strided buffer are sent back to back, because the panel wraps to the next row of the window by itself. The pixel formats are `rgb565Swapped`, which is 16 bits per pixel with the high byte first, and `rgb888`. Native `rgb565` would put the wrong byte first on a little-endian target, and `hal::Display` does not convert pixels.
- `drivers::MipiDsiVideoPanel` initializes a panel that the host streams a frame buffer to. It has no pixel path. It starts the stream after the panel left sleep and before the display is turned on, and stops it after the display was turned off.

A panel is described by a `constexpr` `Panel`: its size, the address mode, the commands to send before and after sleep out, an optional identification to verify, and the timings. A command is either a DCS command or a generic write. A generic command carries its complete payload, register included.
Initialization runs in this order: reset, identification, the commands before sleep out, sleep out and its delay, pixel format, address mode, the commands after sleep out, display on. Without a reset pin, a soft reset is sent instead.
`onInitialized` reports `InitializationResult::noResponse` or `unexpectedId` when the identification cannot be read or differs, and then sends nothing more.
`Sleep()` turns the display off and enters sleep mode, `Wake()` exits sleep mode and turns the display on, and both wait for the delays in `Timings`, which default to the 120 ms that the DCS specification asks for around sleep in and sleep out. `SetBrightness()` writes the display brightness.

When the display is given a tearing effect pin, a write sets the window, waits for the next rising edge of that pin, and only then sends the pixels. The interrupt is enabled only while a write waits. A write that sees no edge within `Timings::tearingEffectTimeout` continues without synchronization, because `hal::Display` has no way to report the failure.

Initialization, `Sleep()`, `Wake()`, `SetBrightness()` and a write each own the host until they complete. Starting one while another is in progress is a programming error, so a caller serializes them with the completions. `Wake()` does not redraw the panel, so a panel that loses its frame memory in sleep has to be redrawn by the application.

## Scanning out a frame buffer

`hal::DisplayController` scans layers out of frame buffers to a panel, and `hal::Blitter` fills, copies and blends rectangles of memory.
Both are for controllers that read the pixels from memory themselves, such as an LCD-TFT controller with a 2D accelerator, and a `hal::DsiVideoStream` host takes its pixels from the same layers. `hal::Display` stays the interface for controllers that are written to.

Pixels in memory are described by `hal::Surface`, a view of a buffer with a size, a format and the distance between rows. `hal::SurfaceFormat` names the layout in memory and is not `hal::PixelFormat`, which names the bytes that a display bus expects. `SubSurface()` cuts a window out of a surface, so an operation takes no rectangle of its own.

- Scan-out runs from construction. `Start()` and `Stop()` only decide whether `onVerticalBlank` and `onUnderrun` are called. `onVerticalBlank` is called once per frame.
- `ConfigureLayer()`, `SetFramebuffer()` and `DisableLayer()` stage a change. `Commit()` applies all staged changes at the next vertical blank and then calls `onApplied`, so swapping the buffers of several layers cannot tear. At most one commit is in flight. `SetPalette()` is the exception and takes effect at once.
- The memory of a layer must stay valid while the layer shows it. After `onApplied` the previous frame buffer is no longer read and can be rendered into.
- At most one blit is in flight per blitter, and a caller that needs several chains them from `onDone`. `Supports()` tells whether an operation handles a pair of formats, so a caller falls back to rendering the rest in software.
- For a blend, `Supports()` is asked about the format of the foreground and the format of the destination. The background must hold colour directly, which `hal::IsDirectColour()` tells, and any such background is accepted whenever the foreground and the destination are supported.
- Callbacks are scheduled on the event dispatcher and never called from within the call that caused them.
- A frame buffer and the surfaces of a blit must be in memory that the scan-out engine and the accelerator reach. Neither interface cleans or invalidates a data cache, so a buffer is either in uncached memory or the caller maintains the cache around a commit and a blit.

A graphics library maps onto the interfaces as follows:

| Graphics library                                                                                                                                                                                    | Interface                                                                     |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------|
| Display size: LVGL `lv_display_create`, TouchGFX HAL width and height, Embedded Wizard `EwBspDisplayInit`                                                                                           | `DisplayController::Size()`                                                   |
| Frame buffers and their stride: LVGL `lv_display_set_buffers`, TouchGFX `setFrameBufferStartAddresses`                                                                                              | `DisplayLayer::framebuffer`                                                   |
| Showing a finished frame and learning when the old buffer is free: LVGL flush and `lv_display_flush_ready` in direct mode, TouchGFX `setTFTFrameBuffer`, Embedded Wizard `EwBspDisplayCommitBuffer` | `SetFramebuffer()` and `Commit()`, with `flush_ready` called from `onApplied` |
| Vertical sync: TouchGFX `vSync` and `frontPorchEntered`                                                                                                                                             | `Start()` with `onVerticalBlank`                                              |
| Colour table: Embedded Wizard `EwBspDisplaySetClut`, TouchGFX L8                                                                                                                                    | `SetPalette()`                                                                |
| Flushing a rendered area: LVGL flush in partial mode                                                                                                                                                | `Blitter::Copy()` into `SubSurface()` of the frame buffer                     |
| Accelerated fill, copy and blend, and the capabilities query: LVGL draw units, TouchGFX `getBlitCaps`, Embedded Wizard bitmap operations                                                            | `Blitter::Fill()`, `Copy()`, `Blend()` and `Supports()`                       |

A library that expects to be called in interrupt context, or one that needs the current line of the panel, is not served by this interface.

## Capturing camera frames

`hal::Camera` describes how frames move from a capture peripheral into a caller-supplied buffer. It covers only the frame-transport path. Sensor configuration — XCLK generation, reset and power sequencing, crop windows, polarity, bus width, exposure and white balance — is the concern of the driver or the application.

A sensor driver implements `hal::Camera` on top of the capture peripheral's `hal::Camera` instance, the same way a codec driver sits on top of `hal::AudioOutput`.

`drivers/camera/omnivision` holds the parts that sensors from OmniVision share. They are configured over SCCB, a bus that is almost I2C:

- `drivers::SccbBusAccessI2c` is a register bus on an I2C master. It puts a stop between the register address and the data of a read, as SCCB requires, where `hal::I2cMasterRegisterAccess` uses a repeated start. Registers are one byte wide and there is no auto-increment.
- `drivers::RegisterTableRunner` walks a `constexpr` table of write, read-modify-write and delay steps in place. The table is not copied and has no size limit, which `services::RegisterStepRunner` does not offer.
- `drivers::OmniVisionSensor` is the `hal::Camera` of a sensor. It powers up and resets the sensor, checks the product identification, runs the tables of its `Descriptor` in the order base, format, resolution, options and tuning, and only then reports `InitializationResult::success`. A sensor with another identification stops the initialization with `unexpectedId` and writes nothing more. After that it forwards `Start()` and `Stop()` to the capture peripheral.

A sensor for a specific chip derives from `drivers::OmniVisionSensor` and supplies a `Descriptor` with the identification registers and the register tables for the chip. The library ships no register tables for specific chips; they come from the documentation of the sensor vendor.

- `CameraFormat` names the pixel layout and the frame dimensions. `BytesPerPixel()` gives the fixed byte count per pixel for uncompressed formats. JPEG has no fixed frame size, so `BytesPerPixel()` returns 0, `IsCompressed()` returns true, and `FrameSizeInBytes()` returns 0. For uncompressed formats `FrameSizeInBytes()` gives `width × height × BytesPerPixel`.
- The caller allocates a buffer and passes it to `Start()`. For uncompressed formats the buffer must hold at least `FrameSizeInBytes()` bytes; `IsValidFrameBuffer()` checks this with 64-bit arithmetic so large dimensions do not overflow. For JPEG any non-empty buffer is valid; the real compressed size is reported as the frame length in the callback.
- Placement in DMA-capable memory and any cache maintenance required before and after a DMA transfer are the concern of the vendor implementation.

**Snapshot vs continuous**

- `Mode::snapshot` captures one frame. When the frame arrives the camera stops automatically and `Start()` is valid again immediately, including from within the `onFrame` callback.
- `Mode::continuous` overwrites the same buffer every frame. A frame is valid only until the next one begins arriving, so a consumer that needs it longer copies it, or uses snapshot mode. A multi-buffer ring is not part of the interface.

**Callbacks and Stop**

Callbacks are scheduled on the event dispatcher, never called from within `Start()`. `Stop()` is idempotent and safe to call from within `onFrame` or `onError`. No callback fires after `Stop()` returns. A `Start()` immediately after `Stop()` is always accepted.

**Errors**

- `Error::overrun` covers buffer-too-small situations as well as DMA overflows. In snapshot mode the capture ends after an overrun; in continuous mode the implementation resumes at the next frame.
- `Error::synchronization` signals a lost sync signal. The same per-mode rule applies.
- A lost frame produces no callback and no error. Repeated absence of any callback indicates a dead or stalled sensor; the caller is expected to arm a timer and call `Stop()` to handle the timeout.

**What this interface does not cover**

XCLK generation, reset and power sequencing, crop and polarity settings, bus width, exposure, white balance and frame-rate control are all outside `hal::Camera`. A vendor peripheral driver exposes only the frame-capture path; a sensor driver stacks on top of that to configure the sensor chip.

## Reading a touch screen

`hal::TouchScreen` reports the touches on a single-touch panel as events. It covers only the position path. Powering the panel, the display behind it and gestures are separate concerns of the driver or the application.

- `Size()` states the range of the reported coordinates: x lies in `[0, width)` and y in `[0, height)`. The unit is that of the touch sensor, which is not necessarily the pixel of the display behind it. A consumer scales and orients the points to its display.
- `Start()` registers the callback and `Stop(onStopped)` removes it. Events are delivered from the event dispatcher, never from within `Start()` or `Stop()`.
- A touch is one `pressed`, any number of `moved` and one `released`. `moved` is only reported when the position changed. `released` repeats the last position, so a consumer can recognise a tap without remembering the point itself.
- `Stop()` ends a touch that is in progress without a `released`. A `Start()` while the panel is still touched reports that touch as a new `pressed`.
- `Stop(onStopped)` may be called from within the callback. No event is delivered after `Stop()` returns. `onStopped` is called from the event dispatcher once the implementation has no hardware transaction left. Start again, call `Stop()` again and destroy the implementation only after that.

`drivers::Stmpe811` is the `hal::TouchScreen` of the STMPE811 touch screen controller, which converts the four wires of a resistive panel. It is reached through a `services::RegisterBusAccess`; `drivers::Stmpe811BusAccessI2c` provides one on an I2C master, at the address that the `ADDR0` pin selects.

- The constructor resets the device, checks its chip identification and configures the converter and the touch screen controller. It reports `InitializationResult::success` when the device is ready, or `deviceNotFound` and writes nothing more when the identification differs. `Start()` is only valid after a success.
- The points are the 12-bit position as the converter delivers it, so `Size()` is 4096 by 4096. The pressure is not reported. Calibration against a display, such as the offset and the axis orientation of the panel, is up to the consumer.
- The interrupt pin, which is active low, wakes the driver for a new touch. A touch that is in progress is polled every `Config::pollInterval`, because the device raises no interrupt when the pen is lifted. With `hal::dummyPin` as interrupt pin the device is polled all the time.
- A sample is read together with a flush of the FIFO of the device, so the next sample is always a fresh one and a stale sample of an earlier touch never becomes the `pressed` of the next.
- The device keeps sampling while the driver is stopped. `Start()` therefore flushes the FIFO in its first poll, before the status is read, and a touch that is already in progress is reported as `pressed` from a fresh sample one poll interval later.
- `Stop(onStopped)` reports the stop when the sample cycle that is in progress, or the initialization, has finished. The result of such a cycle is discarded. Calling `Stop()` before the initialization has finished is accepted.

`drivers::Ft6x06` is the `hal::TouchScreen` of the FocalTech FT6x06 capacitive touch screen controller. It is reached through a `services::RegisterBusAccess`; `drivers::Ft6x06BusAccessI2c` provides one on an I2C master, at address 0x38 unless another is given.

- The controller needs no configuration. The constructor reads the vendor id and the chip id and reports `InitializationResult::success` when the vendor id is neither 0x00 nor 0xff, which is what an empty or a floating bus reads as. A controller that shares its reset with the display behind it can take a while after that reset to answer, so the identification is repeated every `Config::identificationRetryInterval` up to `Config::identificationAttempts` times before `deviceNotFound` is reported. `Start()` is only valid after a success. `VendorId()` and `ChipId()` return what was read.
- The points are in the coordinates of the panel, `Config::size`, which is 480 by 800 for the portrait panel of the MB1166 display module. A coordinate beyond the size is limited to the last one. `Config::orientation` matches the points to a display that is driven in another orientation: the axes are swapped first, when `swapAxes` is set, and then x and y are mirrored with `mirrorX` and `mirrorY`. `Size()` reports the size after the swap. The MB1166 driven in landscape needs `swapAxes` and `mirrorY`, which gives x from 0 to 799 and y from 0 to 479. Without it the points are those of the panel.
- The touch status is polled every `Config::pollInterval`, because the controller is read without its interrupt pin. Only the first touch point is reported; a second finger does not change what is reported, and a touch count above two, which the controller reports while it starts, is ignored.
- `Stop(onStopped)` reports the stop when the status read that is in progress, or the identification, has finished. The result of such a read is discarded, but the result of an identification is still reported through `onInitialized`. Calling `Stop()` before the identification has finished is accepted.
