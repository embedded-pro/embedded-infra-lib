# Display interface for `hal/`: research and evaluation

Status: `hal::Display` and an SSD2119 driver with a BOOSTXL-K350QVG-S1 configuration are implemented but not validated on hardware. The rest of this document is the research behind them. Date: 2026-10-06.

Goal: decide what a display abstraction in `hal/` should look like so that it can sit under LVGL, STemWin, TouchGFX and similar GUI libraries, and so that it can drive controllers such as the ILI9340 and the SSD2119.

## 1. Outcome

The review of this document led to a smaller design than the first proposal, which had two layers (`hal::DisplayBus` and `hal::Display`).

- **One HAL interface.** `hal::Display` is the only new interface in `hal/interfaces/`. It writes a rectangle of pixels from a caller-owned buffer to a display, with an optional row stride, and reports completion asynchronously.
- **No display-specific transport.** The proposed `hal::DisplayBus` was dropped. The repository already has `services::RegisterBusAccess`, a register index plus data bytes, which the IMU drivers use for chips that sit on more than one bus. A controller driver takes a `RegisterBusAccess` and brings its own bus adapter. For the SSD2119 on the BOOSTXL-K350QVG-S1 that adapter is `Ssd2119BusAccessSpi`. A parallel adapter belongs in a platform repository.
- **SSD2119 first.** The MIPI DCS core with an ILI9340 driver, and an LVGL host example, were left out of this change. `hal::Display` was designed to fit them (section 7.5) and nothing in it is specific to the SSD2119.

Built:

| Piece | Where |
|---|---|
| `hal::Display`, `IsValidDisplayWrite`, `BytesPerPixel`, mock, and a stub with an in-memory framebuffer | `hal/interfaces/` |
| `drivers::Ssd2119`: reset, timer-driven bring-up, window and counter handling, mirrored axes, strided writes | `drivers/display/ssd2119/` |
| `drivers::Ssd2119BusAccessSpi`: register index and data over 4-wire SPI with the data/command line | `drivers/display/ssd2119/` |
| `drivers::boostxlK350qvgS1Panel` and `drivers::BoostxlK350qvgS1`: the board configuration and a class that wires the adapter and the driver | `drivers/display/ssd2119/` |

Rules that decided the design:

1. The primary operation is a **rectangle write from a caller-owned buffer with an asynchronous completion**. It is the only operation every surveyed library can be mapped onto without cost.
2. **Controller command grammar never appears in `hal::Display`.** The ILI9340 uses 8-bit commands with byte parameters, and the SSD2119 uses a register index plus 16-bit data and a different address model. Only the controller driver knows this.
3. **Pixel format and byte order are advertised, not converted.** The HAL does not touch pixels. The GUI adapter picks the matching format (for example LVGL's `RGB565_SWAPPED`).

Biggest risks, detailed in section 8:

- LVGL and TouchGFX wait for a flush in a spin or semaphore. That deadlocks if the completion callback is only delivered by the same event dispatcher that is blocked. This was documented in `docs/ExecutionModel.md` but not prototyped.
- STemWin's FlexColor path and the uGFX/Adafruit-style libraries are synchronous. They need an adapter thread.
- Nothing was run on hardware (section 12).

## 2. How this was researched

- The repository was read for conventions: `hal/interfaces`, `hal/synchronous_interfaces`, `drivers/imu`, `services/util`, `services/peripheral`, the test doubles, and `docs/ExecutionModel.md`.
- Two web research passes covered the GUI libraries and the controllers. Several hosts were blocked by the egress proxy: the LVGL, SEGGER, TouchGFX and Qt documentation sites, and every datasheet host. A third pass fetched TI's reference driver for the BoosterPack (section 12).
- Each fact below carries a confidence tag:
  - **[P]** read from a primary source (header, source, doc source on GitHub).
  - **[S]** taken from a search summary of an official page that could not be fetched.
  - **[U]** unverified, from memory or inference.
- The research passes read **no datasheet**. Controller facts came from driver source: Zephyr, Linux DRM, TI TivaWare's Kentec SSD2119 driver, ST's BSP, Espressif, TFT_eSPI. The SSD2119 datasheet and the board user guide were supplied afterwards, and section 5.2 and section 12 are corrected against them. The ILI9340 and ILI9341 facts in section 5.1 are still from driver source only. What is still open is listed in section 9.

## 3. What the repository offers today

- **No display, graphics, framebuffer or pixel type exists** outside `services/terminal` (a text terminal model, unrelated).
- **No parallel bus** (FMC/FSMC/8080) exists. `hal::SpiMaster` is the only bus a display could use today. It moves bytes and has no frame-size control of its own. A platform `CommunicationConfigurator` might provide 9-bit frames, but that is not part of the interface.
- **Conventions a new interface must follow** (`CLAUDE.md`, `hal/interfaces/Watchdog.hpp`, `AudioOutput.hpp`):
  - protected default constructor and non-virtual destructor, deleted copy operations;
  - completion through `const infra::Function<void()>&`. The default `infra::Function` capture storage is only `2 * sizeof(void*)`, so completion lambdas must capture at most two pointers;
  - one outstanding operation, enforced with `really_assert` (see `L3gd20BusAccessSpi`);
  - a non-virtual convenience overload on top of one virtual, as `hal::SpiMaster::SendData` does over `SendAndReceive`;
  - every new interface ships a `*Mock.hpp` and, where useful, a `*Stub` (the audio commit `d954dc5` is the template: interface, mock, stub, tests, `docs/ExecutionModel.md` paragraph).
- **Existing pieces a display reuses:**
  - `hal::SpiMaster` with `SpiAction::continueSession` keeps chip select asserted across a command phase and a data phase, and `services::SpiMasterWithChipSelect` supplies the chip select.
  - `hal::OutputPin` can drive D/C and reset.
  - `hal::SingleChannelPwm` with `hal::DutyCycle` covers a PWM backlight, so **no backlight interface is needed**.
  - `hal::GpioPin::EnableInterrupt` covers a tearing-effect (TE) pin, so **no TE interface is needed**.
  - `services::RegisterBusAccess` (an 8-bit register address plus data bytes, with an `onDone` callback) is the register-access seam of the IMU drivers, and it fits both a DCS command with its parameters and an SSD2119 register with its 16-bit value. `services::RegisterStepRunner` is the precedent for a timer-driven init sequence, but it holds at most 20 steps in RAM and needs an `AccessedBySharedPtr`, so the SSD2119 driver sequences its own table instead.
  - `hal/synchronous_interfaces` exists "only for bootloaders or contexts without an event dispatcher".
- **Constraints that bite:** no heap, no blocking, no sleeping. Every controller wait (reset, 120 to 200 ms after sleep-out for DCS controllers, 30 ms for the SSD2119) must be a timer step. LVGL's own MIPI `init()` blocks for about 510 ms, so it cannot be used as is.

## 4. What the GUI libraries need

### 4.1 Per library

| Library | What it calls into | Sync or async | Buffer ownership | Pixels |
|---|---|---|---|---|
| **LVGL 9.x** [P] | `flush_cb(display, area, px_map)` with an inclusive area. `lv_display_flush_ready()` signals completion and is ISR-safe. Optional `lv_lcd_generic_mipi` driver built on `send_cmd(cmd, params)` (must be synchronous) and `send_color(cmd, buf)` (may be DMA). | Async-capable. A single buffer makes LVGL **spin** until `flush_ready` unless `flush_wait_cb` is set. | Application owns buffers. `px_map` must stay valid until `flush_ready`. Alignment asserted (default 4 bytes). | RGB565 (little-endian), `RGB565_SWAPPED` (byte-swapped, for SPI), RGB888, XRGB8888, ARGB8888, L8, I1. Software or MADCTL rotation. No backlight, power or TE API. |
| **STemWin / emWin** [P for headers, S for controller list] | Either `GUIDRV_FlexColor` fed by `GUI_PORT_API` (16-bit command cycle, data cycle, bulk write, bulk read, CS) or `GUIDRV_Lin` over a framebuffer. FlexColor knows ILI9341 natively. ILI9340 and SSD2119 are listed in the manual [S], with `GUIDRV_FLEXCOLOR_F667xx` numbers [U]. | **Synchronous**, called from inside `GUI_Exec()` / `GUI_Delay()`. No completion callback. | emWin owns its memory pool. Port functions are blocking. | `GUICC_565`, 16-bit words. The port decides wire order (high byte first on 8-bit SPI). Rotation via `CONFIG_FLEXCOLOR`. |
| **TouchGFX** [S, no header read] | Partial-framebuffer strategy: `TransmitBlock(pixels, x, y, w, h)`, `TransmitActive()`, completion calls `startNewTransfer()`. Full-framebuffer strategy: `flushFrameBuffer(Rect&)`. TE as VSYNC. | Owns the main loop (`taskEntry` never returns). Needs OS semaphores. | Application supplies framebuffer or blocks. | RGB565 usual. Byte order unverified. |
| **ThreadX GUIX** [P] | `buffer_toggle(canvas, dirty_rect)` per display driver. | Synchronous. | Canvas owned by the application. | RGB565 and others. |
| **uGFX** [S] | `gdisp_lld_write_start/color/stop`, plus a board file for pins. | Synchronous. | n/a | Driver-defined. |
| **Adafruit_GFX / TFT_eSPI** [P] | `setAddrWindow` + `writePixels(uint16_t*, len, bigEndian)`. Only `drawPixel` is mandatory. | Synchronous, optional DMA with a poll. | Caller. | `uint16_t` RGB565 with an endian flag. |
| **U8g2** [P] | Message callback: `DRAW_TILE` (8x8 monochrome), byte-send and D/C messages. | Synchronous, needs a blocking delay. | Library keeps the page buffer. | Monochrome only. |

### 4.2 Consolidated requirements

| Capability | LVGL | emWin | TouchGFX | GUIX | Adafruit/TFT_eSPI | Verdict for `hal::Display` |
|---|---|---|---|---|---|---|
| Write a pixel rectangle from a caller buffer | Required | Required (built from raw cycles) | Required | Required | Required | **Core** |
| Asynchronous completion | Optional | No | Required for partial strategy | Optional | Optional | **Core** |
| Stride (rectangle inside a larger buffer) | Required in DIRECT mode | n/a | Required for full framebuffer | Required | n/a | **Core** |
| RGB565 in both byte orders | Both | 16-bit words | Unverified | Unverified | Flag | **Core, as a format query** |
| Raw command/parameter/data transport | Used by its MIPI driver | Required (A0/A1 cycles) | App code | No | Required | **Bus layer** |
| Resolution query | Required | Required | Required | Required | Required | **Core** |
| GRAM read-back | No | Required unless cached (`C1`) | No | No | Optional | **Optional, bus layer** |
| Hardware rotation or mirror | Optional | Optional | Unverified | Optional | Optional | Optional, narrow interface |
| Display on/off | No | Optional | No | No | No | Optional, narrow interface |
| TE or VSYNC | Optional | Optional | Required | Optional | No | Existing `GpioPin` interrupt |
| Backlight | No | No | No | No | No | Existing PWM, **not** the display |
| Last chunk of a frame | Optional | No | No | No | No | Open question (section 9) |
| Touch | Required (indev) | Required | Required | Unverified | No | **Separate interface, out of scope** |

Key takeaways:

- Nothing needs per-pixel drawing. The GUI libraries render themselves, so a pixel-primitives API (`DrawPixel`, `FillRect`, ...) is not needed. On the ILI9341 a single pixel costs about 6.5 times the cost of a streamed one, because the window command is 11 bytes of overhead.
- LVGL's own MIPI driver shows what the minimum transport is: `send_cmd` and `send_color`.
- Hardware framebuffer panels (LTDC) fit too. A memory-backed adapter that implements `hal::Display::Write` as a copy is trivial and avoids a second interface for the first version.

## 5. What the controllers need

### 5.1 ILI9340 and ILI9341 (one family, MIPI DCS style)

- 240 x 320, 16 bpp (COLMOD `0x55`) and 18 bpp (`0x66`), no 24 bpp. A 3-byte-per-pixel buffer is carried as 18-bit (Zephyr).
- Host interfaces: 4-wire SPI with a D/C pin, 3-wire SPI (9 clocks per byte), 16/18-bit RGB parallel. 8080 8/9/16/18-bit parallel is [U].
- Commands: 8-bit with 0 to N byte parameters. Window is CASET `0x2A` and PASET `0x2B`, 4 bytes each, big-endian, then RAMWR `0x2C` followed by a pixel stream: three commands and 8 parameter bytes.
- Orientation: MADCTL `0x36` (MY `0x80`, MX `0x40`, MV `0x20`, ML `0x10`, BGR `0x08`, MH `0x04`).
- Read-back: ID registers, status, GRAM read (needs MISO wired). ID `0x9341` is [U].
- Waits: about 5 ms after reset, 120 ms minimum after sleep-out. Real drivers use 120 to 200 ms.
- Pixel bytes go out high byte first.
- Init is about 20 to 25 commands and about 110 parameter bytes, including undocumented vendor registers in the Adafruit table.
- **ILI9340 versus ILI9341:** the command model is the same. They differ in the init table (Zephyr shares one `ili9xxx` core per variant). Exact electrical differences are [U].
- Clock: 10 MHz is the documented [U] limit, 40 MHz works in practice (TFT_eSPI).

### 5.2 SSD2119 (index plus data style, read from the datasheet)

- 320 RGB x 240 with 172,800 bytes of GDDRAM, 65k or 262k colours. Interfaces: 8, 9, 16 and 18-bit 6800 or 8080 parallel, 3-wire and 4-wire serial, and an RGB interface, selected by the `PS[3:0]` pins.
- Each register write is two bus phases: an **index** write with the data/command line low, then the **16-bit value** with it high. There is no `0x2C`-style memory command. Pixel data is register `R22h`: write the index `0x22`, then stream the pixels.
- **Window and address counter are separate.** `R44h` holds the vertical end in its high byte and the start in its low byte, `R45h` and `R46h` the horizontal start and end, and the window only bounds the auto-increment. `R4Eh` (X) and `R4Fh` (Y) set the address counter, which must be placed in the window explicitly. The counter wraps inside the window, so a packed write needs one `R22h` stream.
- **Orientation has no MADCTL equivalent.** `R11h` has `ID1` and `ID0` (vertical and horizontal counter: increment when set, decrement when clear) and `AM` (horizontal first when clear). `R01h` has scan-direction bits as well. A panel whose first pixel is the last GDDRAM position is driven with both counters decrementing and mirrored addresses.
- **Colour mode:** `R11h` `DFM` = 11 is 65k and 10 is 262k. On the 8-bit and SPI interfaces 65k is two bytes (`R4..R0 G5..G3`, then `G2..G0 B4..B0`) and 262k is three bytes with each colour in the upper six bits. On the 16-bit parallel interface 65k is one word.
- **Device code** `R00h` reads as `0x9919`. Registers can be read on parallel and 4-wire SPI interfaces when the data output is wired, and reads are slow (a 450 ns read cycle against 75 ns for a write).
- **4-wire SPI:** at most 15 MHz, reset pulse at least 15 us, and the framing shows the chip select around each byte. The reference drivers hold it for a whole operation instead (section 12.3).
- Bring-up is about 25 register writes, with a 30 ms wait after leaving sleep mode and `R07h` = `0x0033` to switch the display on.
- **STM32's BSP does not ship an SSD2119 driver.** SSD2119 code exists in the legacy STM32F4-Discovery libraries, the Silicon Labs Gecko SDK, TI's TivaWare and MSP430ware, and the ADI MSDK.
- Not found in the datasheet: a tearing-effect output and an integrated backlight driver. The backlight is external.

### 5.3 Others that must stay in reach

| Chip | Command model | Notes |
|---|---|---|
| ST7789, ST7735, ILI9488, HX8357D | DCS | RAM offsets on odd panels (use a gap or offset), ILI9488 needs 3 bytes per pixel on SPI, HX8357D needs a vendor unlock |
| SSD1963 | DCS-like plus vendor | PLL lock waits, own framebuffer, integrated PWM backlight, parallel only [U] |
| SSD1306 | Own opcodes, control byte | Monochrome, 8-pixel pages, I2C or SPI, rectangle must be page aligned |
| SSD1351 | Own opcodes | Command then parameters, shape like DCS |
| LTDC / DSI | None, or DCS over DSI | Host scans a framebuffer, no window write |

### 5.4 What this forces

1. **A seam between the command grammar and the bus.** The transport (D/C pin, 9-bit D/C-in-stream, parallel RS, I2C control byte) is independent of the command grammar (DCS bytes versus index plus word versus own opcodes). The same chip sits on several transports, and the same transport serves several chips. Zephyr (`display` plus `mipi_dbi`), ESP-IDF (`esp_lcd_panel_t` plus `esp_lcd_panel_io_t`), Linux DRM, Rust (`display-interface`), Arduino_GFX and ST's BSP all converged on such a split. In this repository the seam already exists as `services::RegisterBusAccess`, so no display-specific transport is needed (section 1).
2. **The window operation is not portable.** It is three commands for DCS, six index/data pairs for the SSD2119, and a page-aligned range for the SSD1306. It belongs in the controller driver.
3. **All timing must be asynchronous.** Resets and sleep-out waits become timer steps.
4. **Byte order and word width belong to the bus adapter.** MSB first on SPI, a 16-bit word on a 16-bit bus. The driver is configured with the pixel format that its adapter puts on the wire.
5. **Reads are optional.** MISO is often not wired, reads are slow, and they are needed only for an ID check and for STemWin without a cache.

### 5.5 Throughput (16 bpp, 320 x 240 = 153,600 bytes)

| SPI clock | Full frame | Frame rate |
|---|---|---|
| 10 MHz | 122.9 ms | 8.1 |
| 20 MHz | 61.4 ms | 16.3 |
| 40 MHz | 30.7 ms | 32.6 |
| 80 MHz | 15.4 ms | 65 |

- A full 16 bpp frame is 150 KiB. Many MCUs cannot hold it, so GUI libraries render in strips (a 10-row strip is 6,400 bytes, 1.28 ms at 40 MHz), and a strip flush needs an asynchronous completion.
- 3-wire SPI is 12.5% slower (9 clocks per byte). RGB666 over SPI costs 3 bytes per pixel.

## 6. Options considered

| # | Option | Compatibility | Controller reuse | Fit with EmIL | Verdict |
|---|---|---|---|---|---|
| 1 | One monolithic `hal::Display` (Zephyr display API shape), each driver talks to `hal::SpiMaster` directly | Good for LVGL, TouchGFX, GUIX | Poor: ILI9340 on SPI and on FMC means two drivers, and SSD2119 (parallel) has no bus at all | Fine | Rejected |
| 2 | **Two layers: `DisplayBus` + `Display`** | Good for all, plus direct reuse of LVGL's MIPI driver and STemWin FlexColor through the bus | Good: one DCS core serves ILI9340/9341/ST7789 on any bus | Good: mirrors `RegisterBusAccess` + driver, and `SpiMaster` + `SpiMasterWithChipSelect` | **Recommended** |
| 3 | Transport only: copy LVGL's `send_cmd`/`send_color` or STemWin's `GUI_PORT_API` as the HAL, controllers live inside each GUI library | Maximum with each library's built-in drivers | None across libraries: every GUI duplicates controller knowledge, nothing for non-GUI users or tests | Poor (DRY) | Rejected as the whole answer, but covered as the bus layer of option 2 |
| 4 | Framebuffer centric (fbdev / Qt for MCUs style: pointer plus "present") | Good for LTDC panels | n/a | Poor: 150 KiB per frame, SPI panels cannot be served | Rejected for v1, available as a memory-backed adapter |
| 5 | Pixel-primitive API (`DrawPixel`, `FillRect`, ...) | Only Adafruit-style libraries | Weak | Poor: about 6.5x per-pixel cost | Rejected |

**Outcome.** Option 2 was rejected during review in favour of option 1 with the bus handled per driver, because `services::RegisterBusAccess` already is the register-access seam. This keeps one HAL interface and lets the SSD2119 live in this repository, since the BoosterPack is wired for SPI. The cost is that LVGL's generic MIPI driver and STemWin's FlexColor port cannot be handed a HAL bus object. An adapter would wrap the concrete bus type instead.

## 7. As built

```cpp
namespace hal
{
    struct DisplaySize { uint16_t width; uint16_t height; };
    struct DisplayArea { uint16_t x; uint16_t y; uint16_t width; uint16_t height; };

    enum class PixelFormat : uint8_t
    {
        grey8,
        rgb565,          // 16-bit words in native byte order
        rgb565Swapped,   // bytes in wire order, high byte first
        rgb888
    };

    constexpr std::size_t BytesPerPixel(PixelFormat format);
    bool IsValidDisplayWrite(DisplaySize size, PixelFormat format, const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes);

    class Display
    {
    protected:
        Display() = default;
        Display(const Display& other) = delete;
        Display& operator=(const Display& other) = delete;
        ~Display() = default;

    public:
        virtual DisplaySize Size() const = 0;
        virtual PixelFormat Format() const = 0;
        virtual void WriteWithStride(const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone) = 0;

        void Write(const DisplayArea& area, infra::ConstByteRange pixels, const infra::Function<void()>& onDone);
    };
}
```

The strided write has its own name, as `SpiMaster::SendData` and `SendAndReceive` do, so that a derived class that overrides it does not hide the packed `Write`.

### 7.1 Semantics

- **Single flight.** At most one write is outstanding per display. A second call before `onDone` is a programming error (`really_assert`). A completion callback may start the next write. GUI libraries that double-buffer queue on their own side.
- **Buffer lifetime.** `pixels` stays valid until `onDone`. The implementation does not copy.
- **Completion is never invoked from inside the call that started it.** This avoids unbounded recursion. See risk R1 for the consequence.
- **Coordinates.** Origin plus size, so there is no inclusive/exclusive ambiguity (LVGL is inclusive, ESP-IDF is exclusive). An empty area completes without bus traffic. An area outside `Size()`, or a buffer too small for the area and stride, is a programming error. `IsValidDisplayWrite` holds these checks for every implementation.
- **Stride.** `strideInBytes` is the distance between rows in the source buffer. LVGL's DIRECT mode and a TouchGFX full framebuffer need it. `Write` is the packed case.
- **Format.** `Format()` states the layout the display expects. The HAL does not convert. The pixel format of a driver describes what its bus puts on the wire: an 8-bit SPI bus wants `rgb565Swapped`, a 16-bit parallel bus can take native `rgb565`.
- **Initialisation** is not part of the interface. A driver starts its bring-up from its constructor and reports it through an `onInitialized` callback. Writing before that is a programming error.

### 7.2 Optional capabilities as separate interfaces (ISP)

No surveyed library requires these, so they are **not** in `hal::Display`:

- `hal::DisplayPower` (display on/off, sleep) and `hal::DisplayOrientation` (mirror, swap XY), implemented by controller drivers if needed.
- The backlight stays a `hal::SingleChannelPwm` owned by the application.
- Touch becomes a later `hal::Pointer` style interface (`sample` giving x, y, pressed), implemented by drivers such as XPT2046 or FT6x36. LVGL's `indev` read callback, STemWin's `GUI_TOUCH_StoreState` and TouchGFX's `sampleTouch` all map onto it.

### 7.3 Module layout

```
hal/interfaces/Display.{hpp,cpp}
hal/interfaces/test_doubles/DisplayMock.hpp, DisplayStub.{hpp,cpp}   in-memory framebuffer
hal/interfaces/test/TestDisplay.cpp
drivers/display/ssd2119/Ssd2119.{hpp,cpp}                            index/data writes, window and counter, entry mode
drivers/display/ssd2119/Ssd2119BusAccessSpi.{hpp,cpp}                RegisterBusAccess over SpiMaster + data/command pin
drivers/display/ssd2119/Ssd2119BoostxlK350qvgS1.{hpp,cpp}            board configuration and wiring class
docs/ExecutionModel.md                                               completion context and strip-flush guidance
```

- The GUI adapters (`LvglDisplay`, a TouchGFX HAL class, an emWin port) belong **outside** the core library: either a separate repository or `examples/`. Do not link LVGL, STemWin or TouchGFX into `hal` or `drivers`. LVGL is MIT. STemWin and TouchGFX are ST-licensed and described as restricted to STM32 [U, check before shipping adapters].
- Parallel buses (FMC/FSMC or a GPIO bit-bang) are implemented in a platform repository as a `services::RegisterBusAccess`. A memory-mapped FMC bus is almost synchronous (`*indexAddress = x; *dataAddress = y;`), so it can complete from the dispatcher without a DMA path.

### 7.4 How each library binds

| Library | Binding | Work in the adapter |
|---|---|---|
| LVGL (custom flush) | `flush_cb` calls `display.Write({x1, y1, x2-x1+1, y2-y1+1}, px_map, ...)`. The completion lambda captures the `lv_display_t*` and calls `lv_display_flush_ready`. | Format from `Format()` (`RGB565_SWAPPED` for SPI), `lv_tick_set_cb` from the timer service, a repeating timer for `lv_timer_handler`, **R1 below** |
| LVGL (its MIPI driver) | Needs a command/parameter transport, which `hal::Display` does not expose | Not supported through the HAL. Wrap the concrete bus adapter if the built-in driver is wanted |
| STemWin FlexColor | `GUI_PORT_API` cycles map onto register index and data writes of a bus adapter | Blocking: **R2 below** |
| STemWin Lin or custom | memory-backed framebuffer, adapter flushes with `Write` | Needs dirty-area knowledge, unverified |
| TouchGFX partial strategy | `TransmitBlock` maps to `Write`, `TransmitActive` is a flag set by the adapter, completion calls `startNewTransfer()`, the TE pin via `GpioPin::EnableInterrupt` | Needs an OS wrapper (see `osal/`) |
| GUIX | `buffer_toggle(canvas, dirty)` maps to `Write` of the dirty area with the canvas stride | Synchronous: **R2** |
| Adafruit/TFT_eSPI-like | `setAddrWindow` + `writePixels` map to `Write` | Synchronous: **R2** |

### 7.5 How each controller maps

| Controller | Driver | Window write | Register-access calls |
|---|---|---|---|
| SSD2119 (built) | `Ssd2119` | `R45h`, `R46h`, `R44h`, `R4Eh`, `R4Fh`, then `R22h` | `WriteRegister(index, 2 bytes)` for each, then `WriteRegister(0x22, pixels)` |
| ILI9340/ILI9341 (not built) | a DCS core over `RegisterBusAccess` | `0x2A`, `0x2B`, `0x2C` | `WriteRegister(0x2A, 4 bytes)`, `WriteRegister(0x2B, 4 bytes)`, `WriteRegister(0x2C, pixels)` |
| ST7789, ST7735 (not built) | same core, offsets | same | same, plus gap |
| SSD1306 (not built) | later | page range `0x21`/`0x22` | needs page alignment, a capability the interface lacks today |
| LTDC | memory-backed adapter | copy into the layer buffer | no bus |

## 8. Evaluation against EmIL constraints

| Criterion | Assessment |
|---|---|
| No heap | Satisfied. All buffers are caller-owned or static. The only container in production code is `std::array`. The register tables are `constexpr`, so they live in flash. |
| Non-blocking | Satisfied. Reset and the 30 ms wait are `infra::TimerSingleShot` steps. Writes chain on the bus completions. The adapters for synchronous libraries are the risk. |
| SOLID | `Display` is three virtual methods. Power, orientation, read-back and touch are separate. The driver is constructor-injected with a `RegisterBusAccess`, a reset pin and a panel description. |
| DRY | One window and bring-up implementation serves every SSD2119 panel. A new panel is a table. |
| Footprint | One interface with three virtual functions. A `Function` capture of one pointer fits the default storage. |
| Testability | A `RegisterBusAccessMock` (`StrictMock`) verifies the exact register sequence, and the SPI adapter is tested against `SpiMock` with the data/command level checked at every transfer. A `DisplayStub` with an in-memory framebuffer lets GUI code run on the host. |
| Feasibility | The SPI adapter is built entirely on `hal::SpiMaster`, `hal::GpioPin` and `SpiAction::continueSession`, with no new platform code. 3-wire (9-bit) SPI cannot be expressed through `SpiMaster` itself and is not supported. |
| Throughput | At 15 MHz a full 320 x 240 frame in 65k colour is about 82 ms, and in 262k colour about 123 ms, so a GUI flushes in strips. A strided write costs two extra register writes per row. |

### Risks

- **R1. Completion deadlock with LVGL, and the same shape with TouchGFX.**
  - Per LVGL's docs, it waits for `flush_ready` in a spin loop on a single buffer. With two buffers it should also wait whenever rendering outpaces the flush (to be confirmed in the LVGL source of the version in use). If `onDone` is delivered by the same dispatcher that is running `lv_timer_handler`, it never runs and the system hangs.
  - Mitigations, in order of preference: (a) give the LVGL adapter a `flush_wait_cb` that calls `EventDispatcherWorker::ExecuteFirstAction()` until the flag is set, which is re-entrant and needs a design pass; (b) let a DMA-backed bus signal completion from the interrupt (as `hal::InterruptType::immediate` does for GPIO), documented as a second completion context; (c) always use two buffers and size them so rendering is slower than flushing, which is a tuning rule and not a guarantee.
  - No prototype was made. The contract is documented in `docs/ExecutionModel.md`.
- **R2. Synchronous libraries.** STemWin FlexColor, GUIX, uGFX and Adafruit expect blocking calls. Run the library on an `osal` thread, blocking on a semaphore signalled by `onDone`. This is a standard adapter, outside the HAL.
- **R3. Parallel transport.** The SSD2119 is also sold on 8- and 16-bit parallel buses, which need a `RegisterBusAccess` in a platform repository. How such an adapter groups the data bytes into bus words has to follow the pixel format that the driver is configured with.
- **R4. Register values come from a reference driver.** The panel tuning in the board configuration was not derived from a datasheet, because the datasheet cannot know the panel. See section 12.
- **R5. Licensing.** STemWin and TouchGFX adapters can ship, but not the libraries. The origin of the board's register values needs a decision, see section 12.

## 9. Unverified items and open questions

Resolved by reading the SSD2119 datasheet (rev 1.4) and the BOOSTXL-K350QVG-S1 user guide (SLAU601B): the register map, the window and address-counter rules, the entry-mode bits, the 4-wire SPI framing and timing, the pixel mapping in 8-bit and SPI mode, the display-on sequence, and how the board is wired.

Still open:

1. **Everything on hardware.** The driver was tested against mocks only. See section 12 for the points that need a board.
2. ILI9340/ILI9341 **datasheet values**: none was read, and no ILI9340 datasheet was found.
3. TouchGFX API signatures and byte order: the open repository is archived and the docs were not fetchable.
4. STemWin: the `GUIDRV_FLEXCOLOR_F667xx` number for ILI9340 and SSD2119, and `pfSetCS` / `pfFlushBuffer` semantics.
5. The exact LVGL wait logic in the version in use (read from v9.3 headers and v9.6 docs, so names may have drifted).
6. Whether to include a **last-chunk-of-frame flag** in `Write`. LVGL (`flush_is_last`) and Zephyr (`frame_incomplete`) have one, and e-paper or double-buffered LTDC need an end-of-frame trigger. It was left out of v1.
7. Whether DMA completion may run in interrupt context (R1).

## 10. Status and next steps

Done: `hal::Display` with mock, stub and tests, and the SSD2119 driver, SPI adapter and BOOSTXL-K350QVG-S1 configuration with tests.

Next, in the order that retires the most risk:

1. Run the BoosterPack on a LaunchPad-class board and check the points in section 12.
2. Prototype the LVGL adapter against `DisplayStub` in `examples/`, to settle R1 and the format handling.
3. Add an ILI9340 driver (a MIPI DCS core over `RegisterBusAccess` with a DCS-over-SPI adapter) once its datasheet is at hand.
4. Later: `DisplayPower`, `DisplayOrientation`, a touch interface, an SSD1306 page-aligned variant, and a parallel adapter for the SSD2119 in the platform repository.

## 11. Sources

Primary sources read:

- LVGL: `src/display/lv_display.h`, `src/drivers/display/lcd/lv_lcd_generic_mipi.h` and `.c`, `src/indev/lv_indev.h` (v9.3.0), docs for v9.6.0 (`display/setup`, `color_format`, `rotation`, `refreshing`, `external_display_controllers/gen_mipi`), all under `raw.githubusercontent.com/lvgl/lvgl`.
- STemWin: ST's `LCDConf_FlexColor_Template.c`, `LCDConf_Lin_Template.c`, `GUI_Type.h`, `LCD.h`, `GUIDRV_FlexColor.h`, `GUI.h` from `STM32Cube_FW_F4` (a mirror).
- ThreadX GUIX: `gx_display_driver_565rgb_setup.c`, `gx_api.h`.
- Adafruit_GFX, Adafruit_SPITFT, TFT_eSPI, U8g2 (`u8x8.h`) headers.
- Zephyr: `display.h`, `mipi_dbi.h`, `display_ili9xxx` driver, `ili9340` binding.
- Linux DRM `mipi_dbi` and `mipi_display.h`. ESP-IDF `esp_lcd`. Rust `embedded-graphics`, `display-interface`, `mipidsi`. Arduino_GFX. ST BSP components (`ili9341`, `st7789h2`). TI TivaWare Kentec SSD2119 driver. Silicon Labs and CoX SSD2119 headers.

Secondary only (search summaries, pages not fetchable): TouchGFX support site (SPI, FMC, framebuffer strategies, HAL API), SEGGER knowledge base and UM03001, Qt for MCUs porting guide, uGFX, Embedded Wizard.

Datasheets, not fetched: ILI9341 (`cdn-shop.adafruit.com/datasheets/ILI9341.pdf`), SSD2119 (Crystalfontz SSD2119 page, rev 1.4). No ILI9340 datasheet was found.

Also read: the SSD2119 datasheet (Solomon Systech, rev 1.4, June 2009) and the BOOSTXL-K350QVG-S1 user guide (TI SLAU601B), both supplied with the task. The TivaWare `Kentec320x240x16_ssd2119_spi.c` driver, and for comparison the parallel TivaWare driver and the MSP430ware graphics library files, were read for the panel values and the orientation (section 12).

## 12. BOOSTXL-K350QVG-S1 and the SSD2119 in practice

### 12.1 The board (SLAU601B)

- A Kentec K350QVG-V2-F, 3.5", 320 x 240, 262k colours, with the SSD2119 integrated, a 4-wire resistive touch screen and a white LED backlight.
- Shipped as **4-wire, 8-bit SPI**: `LCD_SCS` chip select, `LCD_SCL` clock, `LCD_SDI` data in, and `LCD_SDC` data/command (data = 1, command = 0), plus `LCD_RESET`. The data output of the controller is not wired, so registers cannot be read. Resistors R2, R3, R8 and R9 select 3-wire, 9-bit SPI, which `hal::SpiMaster` cannot express and the driver does not support.
- The backlight is a boost converter from the 5 V rail, switched with the `LED PWM` pin. The touch screen has four analog lines to the host.

### 12.2 What the datasheet fixes

- **SPI framing:** the register index is one byte sent with `SDC` low and the data bytes follow with `SDC` high, most significant bit first. The clock is at most 15 MHz and the reset pulse at least 15 us. The device code in `R00h` is `0x9919`.
- **Window and counter:** `R44h` holds the vertical end in the high byte and the start in the low byte, `R45h` and `R46h` the horizontal start and end, `R4Eh` and `R4Fh` the X and Y address counter. The window only bounds the auto-increment, so the counter has to be set to the start corner explicitly.
- **Entry mode `R11h`:** `DFM` = 11 selects 65k colours and 10 selects 262k. `ID1` and `ID0` make the vertical and horizontal counter increment when set and decrement when clear, and `AM` = 0 advances horizontally first, wrapping inside the window.
- **Pixel bytes on the SPI interface:** in 65k mode two bytes with `R4..R0 G5..G3` and then `G2..G0 B4..B0`, which is RGB565 with the high byte first, so `hal::PixelFormat::rgb565Swapped` on a little-endian CPU. In 262k mode three bytes, each carrying a colour in its upper six bits, which an `rgb888` buffer provides.
- **Display on:** leave sleep mode, wait 30 ms, then write `R07h` = `0x0033`.

### 12.3 What the reference driver adds

The datasheet describes the controller, not the panel. The power voltages, the common voltage and the gamma curve are tuned per panel, and the only source for them is the reference driver that TI publishes for this BoosterPack (TivaWare's `Kentec320x240x16_ssd2119_spi.c`). `boostxlK350qvgS1Panel` holds those values, in the order that driver writes them, minus the registers that the driver derives from its configuration.

- **Orientation.** The reference driver's default landscape orientation (flexible connector at the bottom) writes entry mode `0x6800` and maps pixel (x, y) to counters (319 - x, 239 - y). By the datasheet, `ID` = 00 makes both counters decrement, so this is a mirrored mapping, which the driver implements with `mirrorX` and `mirrorY`. The reference source does not say which physical corner is pixel (0, 0).
- **Reset.** Low for 10 ms, then 20 ms before the first register write. The datasheet only requires 15 us for the pulse.
- **Pixel format.** 16 bits per pixel, high byte first, in 65k mode. The 262k mode is supported by the driver per the datasheet but TI's code never uses it.
- **Chip select.** TI's variants differ. One toggles chip select around every 16-bit word and sends a command as two bytes, the MSP430 and MSP432 ones hold chip select for a whole operation and send a command as one byte. This driver follows the datasheet and the second form: a single byte at the command level, and chip select held by the `SpiMasterWithChipSelect` session for the index and the data of a register together.
- **SPI mode.** TI uses mode 0 on the Tiva and MSP432, and the MSP430 variant appears to use mode 3. The datasheet figures show the clock idle low with data sampled on the rising edge. Use mode 0.
- **Not done by the bring-up:** clearing the frame memory (TI clears it before the backlight is switched on), and the backlight.

### 12.4 Provenance and licence of the register values

The TivaWare driver carries TI's proprietary software licence, which allows use solely on TI microcontrollers and forbids combining it with copyleft open-source software. This repository is MIT licensed. The code here was written from the datasheet and does not copy that file's code or structure, but the numeric tuning values in `Ssd2119BoostxlK350qvgS1.hpp` were taken from it. A BSD-3-Clause licensed copy of a TI driver for the same display family exists in the MSP430ware graphics library examples, with a different table (`R1Eh` = `0x00B2`, other gamma and power values), which TI's BoosterPack-specific driver does not use. **Decide whether the values may stay, or replace them with values tuned on your own panel**, before publishing.

### 12.5 To check on hardware

1. Which physical corner is pixel (0, 0), and that the image is not mirrored.
2. Red, green and blue are not exchanged (the `BGR` bit of `R01h` is clear) and that a stripe of each primary has the right colour.
3. That holding chip select across the index and the data of a register works, including for a multi-kilobyte pixel write.
4. The SPI mode and the highest clock that does not corrupt pixels.
5. That a strided write, which resets the address counter for every row, produces the right picture.
6. The 262k mode over SPI, if it is wanted.
7. That `Ssd2119BusAccessSpi` works with the `SpiMaster` of the target, in particular that it accepts a transfer as long as the write (a full 320 x 240 frame in 65k colour is 153,600 bytes, which exceeds the 16-bit transfer counter of many DMA engines).
