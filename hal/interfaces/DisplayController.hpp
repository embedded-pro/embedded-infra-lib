#ifndef HAL_DISPLAY_CONTROLLER_HPP
#define HAL_DISPLAY_CONTROLLER_HPP

#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/Surface.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    enum class BlendMode : uint8_t
    {
        constantAlpha, // alpha applies to the whole layer; 255 is opaque
        pixelAlpha     // the alpha of each pixel, scaled by alpha
    };

    struct DisplayLayer
    {
        Surface framebuffer;
        uint16_t x{ 0 };
        uint16_t y{ 0 };
        BlendMode blendMode{ BlendMode::constantAlpha };
        uint8_t alpha{ 255 };
    };

    // Scans layers out of frame buffers to a panel, from construction on.
    // Layer changes are staged, and Commit applies all of them together at the next vertical blank.
    // The memory of a layer must stay valid while the layer shows it
    class DisplayController
    {
    protected:
        DisplayController() = default;
        DisplayController(const DisplayController& other) = delete;
        DisplayController& operator=(const DisplayController& other) = delete;
        ~DisplayController() = default;

    public:
        virtual DisplaySize Size() const = 0;
        virtual std::size_t NumberOfLayers() const = 0;

        // Start and Stop only control whether the callbacks are called. Callbacks are never called from within a call of this interface
        virtual void Start(const infra::Function<void()>& onVerticalBlank, const infra::Function<void()>& onUnderrun) = 0;
        virtual void Stop() = 0;

        virtual void ConfigureLayer(std::size_t layer, const DisplayLayer& configuration) = 0;
        virtual void SetFramebuffer(std::size_t layer, infra::ByteRange framebuffer) = 0;
        virtual void DisableLayer(std::size_t layer) = 0;

        // At most one commit is in flight
        virtual void Commit(const infra::Function<void()>& onApplied) = 0;

        // Takes effect immediately, not at a vertical blank
        virtual void SetPalette(std::size_t layer, infra::MemoryRange<const Argb8888> palette) = 0;
    };
}

#endif
