#ifndef HAL_DISPLAY_CONTROLLER_STUB_HPP
#define HAL_DISPLAY_CONTROLLER_STUB_HPP

#include "hal/interfaces/test_doubles/DisplayControllerMock.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <array>
#include <cstddef>
#include <optional>

namespace hal
{
    class DisplayControllerStub
        : public DisplayControllerMock
    {
    public:
        static constexpr std::size_t maxLayers = 4;

        DisplayControllerStub(DisplaySize size, std::size_t numberOfLayers);

        void VerticalBlank();
        void Underrun();
        void CompleteCommit();

        bool Started() const;
        bool CommitPending() const;
        const std::optional<DisplayLayer>& StagedLayer(std::size_t layer) const;
        const std::optional<DisplayLayer>& AppliedLayer(std::size_t layer) const;
        infra::MemoryRange<const Argb8888> Palette(std::size_t layer) const;

    private:
        void CheckLayer(std::size_t layer) const;

    private:
        DisplaySize size;
        std::size_t numberOfLayers;
        bool started{ false };
        infra::Function<void()> onVerticalBlank;
        infra::Function<void()> onUnderrun;
        infra::AutoResetFunction<void()> onApplied;
        std::array<std::optional<DisplayLayer>, maxLayers> staged;
        std::array<std::optional<DisplayLayer>, maxLayers> applied;
        std::array<infra::MemoryRange<const Argb8888>, maxLayers> palettes;
    };
}

#endif
