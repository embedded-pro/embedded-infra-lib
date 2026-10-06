# Display interface for `hal/`: research and evaluation

Status: proposal, nothing implemented. Date: 2026-10-06.

Goal: decide what a display abstraction in `hal/` should look like so that it can sit under LVGL, STemWin, TouchGFX and similar GUI libraries, and so that it can drive controllers such as the ILI9340 and the SSD2119.

## 1. Recommendation

Add **two small interfaces** to `hal/interfaces/`, with the controller drivers in a new `drivers/display/`:

| Layer | Interface | Answers the question | Implemented by |
|---|---|---|---|
| Transport | `hal::DisplayBus` | How do command, parameter and pixel bytes reach the chip? | SPI+D/C bus (generic, built on `hal::SpiMaster` and `hal::GpioPin`), FMC/FSMC or GPIO parallel bus (platform HAL) |
| Panel | `hal::Display` | Put these pixels in this rectangle and tell me when they are done | `drivers/display/*`: a shared MIPI DCS core (ILI9340/ILI9341/ST7789/...) and an SSD2119 driver |

`hal::Display` is what GUI libraries bind to. `hal::DisplayBus` is what makes the controller drivers independent of SPI versus parallel, and it can also be handed straight to LVGL's generic MIPI driver or to STemWin's `GUIDRV_FlexColor`.

Three rules decide most of the design. They are argued in section 6.

1. The primary operation is a **rectangle write from a caller-owned buffer with an asynchronous completion**. It is the only operation every surveyed library can be mapped onto without cost.
2. **Controller command grammar never appears in `hal::Display`.** ILI9340 uses 8-bit commands with byte parameters, and SSD2119 uses a register index plus 16-bit data and a different address model. Only the controller driver knows this.
3. **Pixel format and byte order are advertised, not converted.** The HAL does not touch pixels. The GUI adapter picks the matching format (for example LVGL's `RGB565_SWAPPED`).

Biggest risks, detailed in section 8:

- LVGL and TouchGFX wait for a flush in a spin or semaphore. That deadlocks if the completion callback is only delivered by the same event dispatcher that is blocked.
- STemWin's FlexColor path and the uGFX/Adafruit-style libraries are synchronous. They need an adapter thread or a synchronous bus.
- The SSD2119 is most often wired in parallel, and the repository has no parallel-bus HAL today.

## 2. How this was researched

- The repository was read for conventions: `hal/interfaces`, `hal/synchronous_interfaces`, `drivers/imu`, `services/util`, `services/peripheral`, the test doubles, and `docs/ExecutionModel.md`.
- Two web research passes covered the GUI libraries and the controllers. Several hosts were blocked by the egress proxy: the LVGL, SEGGER, TouchGFX and Qt documentation sites, and every datasheet host.
- Each fact below carries a confidence tag:
  - **[P]** read from a primary source (header, source, doc source on GitHub).
  - **[S]** taken from a search summary of an official page that could not be fetched.
  - **[U]** unverified, from memory or inference.
- **No datasheet was read.** Controller facts come from driver source: Zephyr, Linux DRM, TI TivaWare's Kentec SSD2119 driver, ST's BSP, Espressif, TFT_eSPI. Anything that has to be right before code is written is listed in section 9.

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
  - `services::RegisterStepRunner` (write, burst, modify, delay, invoke, await steps) is the precedent for a timer-driven init sequence. It is bound to `RegisterBusAccess` (8-bit address and value), so it cannot be reused directly, but a display command-step runner can follow its shape.
  - `hal/synchronous_interfaces` exists "only for bootloaders or contexts without an event dispatcher".
- **Constraints that bite:** no heap, no blocking, no sleeping. Every controller wait (5 ms reset, 120 to 200 ms after sleep-out, 30 ms for the SSD2119) must be a timer step. LVGL's own MIPI `init()` blocks for about 510 ms, so it cannot be used as is.

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

### 5.2 SSD2119 (index plus data style)

- 320 RGB x 240, 65K or 262K colour. Interfaces per the vendor page summary: 8/9/16/18-bit parallel and serial [S/U].
- Each register write is two bus phases: an **index** write with RS low, then a **16-bit data** write with RS high. There is no `0x2C`-style memory command. Pixel data is register `R22h`: write the index `0x22`, then stream words.
- Window and address counter are separate [P, Kentec/TivaWare]. The window registers `R44h` (vertical), `R45h`/`R46h` (horizontal start and end) only bound the auto-increment. The address counter `R4Eh`/`R4Fh` must be set to the start corner explicitly. Entry mode is `R11h`.
- Cost of a window: about six register writes plus the `R22h` index. That is 11 to 13 cycles on a 16-bit bus, or about 16 to 19 byte cycles on an 8-bit bus.
- Orientation has no MADCTL equivalent. Entry-mode increment bits set the direction, and TivaWare mirrors by mapping coordinates in software.
- Device code `R00h` returns `0x9919` [U]. Reads are slow.
- Init is about 30 register writes, with a 30 ms wait after sleep-exit.
- Mostly found on parallel 8/16-bit buses (TivaWare's Kentec driver is 8-bit LIDD). **STM32's BSP does not ship an SSD2119 driver**, so the premise that it does is wrong as far as could be verified.
- Not verified: TE output, integrated backlight, the exact bit semantics of `R11h` (taken from third-party headers).

### 5.3 Others that must stay in reach

| Chip | Command model | Notes |
|---|---|---|
| ST7789, ST7735, ILI9488, HX8357D | DCS | RAM offsets on odd panels (use a gap or offset), ILI9488 needs 3 bytes per pixel on SPI, HX8357D needs a vendor unlock |
| SSD1963 | DCS-like plus vendor | PLL lock waits, own framebuffer, integrated PWM backlight, parallel only [U] |
| SSD1306 | Own opcodes, control byte | Monochrome, 8-pixel pages, I2C or SPI, rectangle must be page aligned |
| SSD1351 | Own opcodes | Command then parameters, shape like DCS |
| LTDC / DSI | None, or DCS over DSI | Host scans a framebuffer, no window write |

### 5.4 What this forces

1. **Two layers.** The transport (D/C pin, 9-bit D/C-in-stream, parallel RS, I2C control byte) is independent of the command grammar (DCS bytes versus index plus word versus own opcodes). The same chip sits on several transports, and the same transport serves several chips. Zephyr (`display` plus `mipi_dbi`), ESP-IDF (`esp_lcd_panel_t` plus `esp_lcd_panel_io_t`), Linux DRM, Rust (`display-interface`), Arduino_GFX and ST's BSP all converged on this split.
2. **The window operation is not portable.** It is three commands for DCS, six index/data pairs for the SSD2119, and a page-aligned range for the SSD1306. It belongs in the controller driver.
3. **All timing must be asynchronous.** Resets and sleep-out waits become timer steps.
4. **Byte order and word width belong to the transport.** MSB first on SPI, a 16-bit word on a 16-bit bus.
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

## 7. Proposed interfaces (sketch)

The sketch follows the repo's conventions. It is a starting point for review, not a final signature.

```cpp
namespace hal
{
    struct DisplaySize
    {
        uint16_t width;
        uint16_t height;

        bool operator==(const DisplaySize& other) const = default;
    };

    struct DisplayArea
    {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;

        bool operator==(const DisplayArea& other) const = default;
    };

    enum class PixelFormat : uint8_t
    {
        monochrome,
        grey8,
        rgb565,          // 16-bit words in native byte order
        rgb565Swapped,   // bytes in wire order, high byte first
        rgb888
    };

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
        virtual void Write(const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone) = 0;

        void Write(const DisplayArea& area, infra::ConstByteRange pixels, const infra::Function<void()>& onDone);
    };

    class DisplayBus
    {
    protected:
        DisplayBus() = default;
        DisplayBus(const DisplayBus& other) = delete;
        DisplayBus& operator=(const DisplayBus& other) = delete;
        ~DisplayBus() = default;

    public:
        virtual void WriteCommand(uint16_t command, infra::ConstByteRange parameters, const infra::Function<void()>& onDone) = 0;
        virtual void WritePixels(uint16_t command, infra::ConstByteRange pixels, const infra::Function<void()>& onDone) = 0;
    };

    class ReadableDisplayBus
        : public DisplayBus
    {
    public:
        virtual void ReadCommand(uint16_t command, infra::ByteRange response, const infra::Function<void()>& onDone) = 0;
    };
}
```

### 7.1 Semantics to specify

- **Single flight.** At most one operation is outstanding per object. A second call before `onDone` is a programming error (`really_assert`), as in `L3gd20BusAccessSpi`. GUI libraries that double-buffer queue on their own side.
- **Buffer lifetime.** `pixels` and `parameters` must stay valid until `onDone`. The implementation does not copy.
- **Completion is never invoked from inside the call that started it.** This avoids unbounded recursion and matches the repo's `Schedule` convention. See risk R1 for the consequence.
- **Coordinates.** Origin plus size, so there is no inclusive/exclusive ambiguity (LVGL is inclusive, ESP-IDF is exclusive). An empty area completes without bus traffic. An area outside `Size()` is a programming error.
- **Stride.** `strideInBytes` is the distance between rows in the source buffer. LVGL's DIRECT mode and a TouchGFX full framebuffer need it. When it differs from the row length, the driver writes row by row (more bus overhead, the driver chooses how). The non-virtual overload is the packed case.
- **Format.** `Format()` states the layout `Write` expects in memory. The HAL does not convert. A 16-bit parallel bus reports `rgb565`, an 8-bit SPI bus reports `rgb565Swapped`. The adapter configures its GUI accordingly. `rgb888` is for panels in 18- or 24-bit mode, with the low bits ignored where the controller only takes 18.
- **Bus command width.** The bus is configured at construction with its command and parameter widths. `parameters` are bytes in wire order. An 8-bit bus sends them as they are, a 16-bit bus groups them as big-endian pairs. This is what lets an SSD2119 register write (`index`, two data bytes) and a DCS command (`0x2A`, four bytes) share one call. It is the least proven part and needs a prototype.
- **Command and pixels are separate calls.** `WritePixels` is the only large, DMA-capable operation. It also carries the command that precedes the stream (`0x2C` for DCS, `0x22` for the SSD2119), so the bus can keep chip select asserted from command to last pixel.

### 7.2 Optional capabilities as separate interfaces (ISP)

No surveyed library requires these, so they are **not** in `hal::Display`:

- `hal::DisplayPower` (`DISPON`/`DISPOFF`/sleep) and `hal::DisplayOrientation` (mirror, swap XY), implemented by the controller drivers.
- Backlight stays a `hal::SingleChannelPwm` owned by the application.
- Touch becomes a later `hal::Pointer` style interface (`sample` giving x, y, pressed), implemented by drivers such as XPT2046 or FT6x36. LVGL's `indev` read callback, STemWin's `GUI_TOUCH_StoreState` and TouchGFX's `sampleTouch` all map onto it.

### 7.3 Module layout

```
hal/interfaces/Display.hpp, DisplayBus.hpp
hal/interfaces/test_doubles/DisplayMock.hpp, DisplayStub.{hpp,cpp}      in-memory framebuffer
hal/interfaces/test_doubles/DisplayBusMock.hpp, DisplayBusStub.{hpp,cpp} records commands/parameters/pixels
hal/interfaces/test/TestDisplay.cpp
services/peripheral/SpiDisplayBus.{hpp,cpp}                              hal::SpiMaster + D/C hal::GpioPin (+ reset)
drivers/display/mipi_dcs/MipiDcsDisplay.{hpp,cpp}                        CASET/PASET/RAMWR, COLMOD, MADCTL, sleep, init steps
drivers/display/ili9340/Ili9340.{hpp,cpp}                                init table on the DCS core (ILI9341, ST7789 as siblings)
drivers/display/ssd2119/Ssd2119.{hpp,cpp}                                index/data writes, window+counter, entry mode
docs/ExecutionModel.md                                                   completion context and strip-flush guidance
```

- The GUI adapters (`LvglDisplay`, a TouchGFX HAL class, an emWin port) belong **outside** the core library: either a separate repository or `examples/`. Do not link LVGL, STemWin or TouchGFX into `hal` or `drivers`. LVGL is MIT. STemWin and TouchGFX are ST-licensed and described as restricted to STM32 [U, check before shipping adapters].
- Parallel buses (`DisplayBus` over FMC/FSMC or a GPIO bit-bang) are implemented in the platform HAL, outside this repository. A memory-mapped FMC bus is almost synchronous (`*commandAddress = x; *dataAddress = y;`), so it can complete from the dispatcher without a DMA path.

### 7.4 How each library binds

| Library | Binding | Work in the adapter |
|---|---|---|
| LVGL (custom flush) | `flush_cb` calls `display.Write({x1, y1, x2-x1+1, y2-y1+1}, px_map, ...)`. Completion lambda captures the `lv_display_t*` and calls `lv_display_flush_ready`. | Format from `Format()` (`RGB565_SWAPPED` for SPI), `lv_tick_set_cb` from the timer service, a repeating timer for `lv_timer_handler`, **R1 below** |
| LVGL (its MIPI driver) | `send_cmd` maps to `WriteCommand`, `send_color` maps to `WritePixels` | Its `send_cmd` must be synchronous, so use only where completion is immediate |
| STemWin FlexColor | `GUI_PORT_API`: A0 cycle maps to `WriteCommand`, A1 cycle to `WritePixels` or `WriteCommand`, bulk write/read to `ReadableDisplayBus` | Blocking: **R2 below** |
| STemWin Lin or custom | memory-backed framebuffer, adapter flushes with `Write` | Needs dirty-area knowledge, unverified |
| TouchGFX partial strategy | `TransmitBlock` maps to `Write`, `TransmitActive` is a flag set by the adapter, completion calls `startNewTransfer()`, TE pin via `GpioPin::EnableInterrupt` | Needs an OS wrapper (see `osal/`) |
| GUIX | `buffer_toggle(canvas, dirty)` maps to `Write` of the dirty area with the canvas stride | Synchronous: **R2** |
| Adafruit/TFT_eSPI-like | `setAddrWindow` + `writePixels` map to `Write` | Synchronous: **R2** |

### 7.5 How each controller maps

| Controller | Driver | Window write | Bus calls |
|---|---|---|---|
| ILI9340/ILI9341 | `Ili9340` on `MipiDcsDisplay` | CASET, PASET, RAMWR | `WriteCommand(0x2A, 4 bytes)`, `WriteCommand(0x2B, 4 bytes)`, `WritePixels(0x2C, pixels)` |
| ST7789, ST7735 | same core, offset parameters | same | same, plus gap |
| SSD2119 | `Ssd2119` | `R44h`, `R45h`, `R46h`, `R4Eh`, `R4Fh` then `R22h` | five or six `WriteCommand(reg, 2 bytes)` then `WritePixels(0x22, pixels)` |
| SSD1306 | later | page range `0x21`/`0x22` | needs page alignment, a capability the interface lacks today |
| LTDC | memory-backed adapter | copy into the layer buffer | no bus |

## 8. Evaluation against EmIL constraints

| Criterion | Assessment |
|---|---|
| No heap | Fully satisfied. All buffers are caller-owned or static. No container beyond `infra::BoundedVector` is needed. |
| Non-blocking | Satisfied by the interfaces. Init waits become timer steps modelled on `RegisterStepRunner`. The adapters for synchronous libraries are the risk. |
| SOLID | `Display` is three methods. Power, orientation, read-back and touch are separate. Controllers are constructor-injected with a `DisplayBus`. |
| DRY | One DCS core serves a family. One SPI bus serves all SPI controllers. |
| Footprint | Two interfaces with about 3 and 2 virtual functions. No templates in the interfaces. A `Function` capture of one or two pointers fits the default storage. |
| Testability | A `DisplayBusMock` (`StrictMock`) verifies the exact byte sequence per controller, with no hardware. A `DisplayStub` with an in-memory framebuffer lets GUI code and the adapters run on the host, in the way `AudioOutputStub` does. |
| Feasibility today | `SpiDisplayBus` can be built entirely on `hal::SpiMaster`, `hal::GpioPin` and `SpiAction::continueSession`, with no new platform code. 3-wire (9-bit) SPI cannot be expressed through `SpiMaster` itself. It would need a software conversion buffer (Linux does this) or a platform configurator, so it should be left out of the first version. |
| Throughput | Adequate: window overhead is 11 bytes for DCS against strip payloads of several KiB. Per-row writes for a strided source are the one place overhead matters. |

### Risks

- **R1. Completion deadlock with LVGL, and the same shape with TouchGFX.**
  - Per LVGL's docs, it waits for `flush_ready` in a spin loop on a single buffer. With two buffers it should also wait whenever rendering outpaces the flush (to be confirmed in the LVGL source of the version in use). If `onDone` is delivered by the same dispatcher that is running `lv_timer_handler`, it never runs and the system hangs.
  - Mitigations, in order of preference: (a) give the LVGL adapter a `flush_wait_cb` that calls `EventDispatcherWorker::ExecuteFirstAction()` until the flag is set, which is re-entrant and needs a design pass; (b) let a DMA-backed `DisplayBus` signal completion from the interrupt (as `hal::InterruptType::immediate` does for GPIO), documented as a second completion context; (c) always use two buffers and size them so rendering is slower than flushing, which is a tuning rule and not a guarantee.
  - This should be settled by a prototype before the signature is frozen.
- **R2. Synchronous libraries.** STemWin FlexColor, GUIX, uGFX and Adafruit expect blocking calls. Options: run the library on an `osal` thread, blocking on a semaphore signalled by `onDone` (a standard adapter, outside the HAL); or provide a `SynchronousDisplayBus` in `hal/synchronous_interfaces` following the repo's own pattern. Duplicating whole controller drivers in a synchronous variant is not recommended.
- **R3. SSD2119 transport.** The parallel bus is not in the repo, and the 16-bit register model needs the bus to know its width. If it cannot be expressed through `WriteCommand(command, parameters)` cleanly, the bus interface needs a register-oriented method.
- **R4. Init sequences are long and undocumented.** The ILI9341 table contains vendor registers with no documented meaning. The tables must be copied from a known-good source (Zephyr, Adafruit) with tests that pin the exact bytes. They need real-hardware validation.
- **R5. Licensing.** STemWin and TouchGFX adapters can ship, but not the libraries. Check terms before publishing them in this repository.

## 9. Unverified items and open questions

Must be resolved before implementation:

1. ILI9340/ILI9341 and SSD2119 **datasheet values**: register addresses and bit meanings (especially `R11h` entry mode and `R4E`/`R4F` ranges), read protocol and dummy reads, max SPI clock, the full reset and sleep timing, whether the SSD2119 has SPI at all. Nothing above was read from a datasheet.
2. TouchGFX API signatures and byte order: the open repository is archived and the docs were not fetchable.
3. STemWin: the `GUIDRV_FLEXCOLOR_F667xx` number for ILI9340 and SSD2119, and `pfSetCS` / `pfFlushBuffer` semantics.
4. The exact LVGL wait logic in the version in use (read from v9.3 headers and v9.6 docs, so names may have drifted).
5. Whether to include a **last-chunk-of-frame flag** in `Write`. LVGL (`flush_is_last`) and Zephyr (`frame_incomplete`) have one, and e-paper or double-buffered LTDC need an end-of-frame trigger. Recommendation: leave it out of v1 and add a `frameComplete` hint when the first consumer needs it.
6. Whether `strideInBytes` should be in the first virtual or a later addition. Recommendation: first, because adding a parameter to a virtual later breaks every implementation.
7. Whether DMA completion may run in interrupt context (R1).

## 10. Suggested plan

1. **Prototype first.** Write the host-side `DisplayBusStub`/`DisplayStub` and an LVGL adapter in `examples/` against them, to settle R1 and the format handling before committing to signatures.
2. Add `Display.hpp`, `DisplayBus.hpp`, mocks and stubs, tests, and the `ExecutionModel.md` section (one PR, following the audio commit as the template).
3. Add `SpiDisplayBus` in `services/peripheral` with tests (byte order, D/C level per phase, chip select across command and data).
4. Add `MipiDcsDisplay` and `Ili9340` with `StrictMock` tests that pin every init byte, window command and wait.
5. Add `Ssd2119` once a parallel `DisplayBus` implementation exists (platform HAL), with tests of the exact register sequence.
6. Later: `DisplayPower`, `DisplayOrientation`, a touch interface, the SSD1306 page-aligned variant, and a synchronous bridge if STemWin demand is real.

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
