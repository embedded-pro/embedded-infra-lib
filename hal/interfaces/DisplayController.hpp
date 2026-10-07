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
        constantAlpha,
        pixelAlpha
    };

    struct DisplayLayer
    {
        Surface framebuffer;
        uint16_t x{ 0 };
        uint16_t y{ 0 };
        BlendMode blendMode{ BlendMode::constantAlpha };
        uint8_t alpha{ 255 };
    };

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

        virtual void Start(const infra::Function<void()>& onVerticalBlank, const infra::Function<void()>& onUnderrun) = 0;
        virtual void Stop() = 0;

        virtual void ConfigureLayer(std::size_t layer, const DisplayLayer& configuration) = 0;
        virtual void SetFramebuffer(std::size_t layer, infra::ByteRange framebuffer) = 0;
        virtual void DisableLayer(std::size_t layer) = 0;

        virtual void Commit(const infra::Function<void()>& onApplied) = 0;

        virtual void SetPalette(std::size_t layer, infra::MemoryRange<const Argb8888> palette) = 0;
    };
}

#endif
