#ifndef HAL_DISPLAY_CONTROLLER_MOCK_HPP
#define HAL_DISPLAY_CONTROLLER_MOCK_HPP

#include "hal/interfaces/DisplayController.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class DisplayControllerMock
        : public DisplayController
    {
    public:
        MOCK_METHOD(DisplaySize, Size, (), (const, override));
        MOCK_METHOD(std::size_t, NumberOfLayers, (), (const, override));
        MOCK_METHOD(void, Start, (const infra::Function<void()>& onVerticalBlank, const infra::Function<void()>& onUnderrun), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, ConfigureLayer, (std::size_t layer, const DisplayLayer& configuration), (override));
        MOCK_METHOD(void, SetFramebuffer, (std::size_t layer, infra::ByteRange framebuffer), (override));
        MOCK_METHOD(void, DisableLayer, (std::size_t layer), (override));
        MOCK_METHOD(void, Commit, (const infra::Function<void()>& onApplied), (override));
        MOCK_METHOD(void, SetPalette, (std::size_t layer, infra::MemoryRange<const Argb8888> palette), (override));
    };
}

#endif
