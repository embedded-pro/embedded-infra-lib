#ifndef HAL_DSI_HOST_STUB_HPP
#define HAL_DSI_HOST_STUB_HPP

#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hal
{
    // Emulates the frame memory of a panel: column and page address, write memory start and write memory continue
    // are decoded into the storage, every other command is ignored
    class DsiHostStub
        : public DsiHostMock
    {
    public:
        DsiHostStub(infra::ByteRange storage, DisplaySize size, PixelFormat format);

        infra::ConstByteRange PixelAt(uint16_t x, uint16_t y) const;

        template<std::size_t StorageSize>
        using WithStorage = infra::WithStorage<DsiHostStub, std::array<uint8_t, StorageSize>>;

    private:
        struct Span
        {
            uint16_t first;
            uint16_t last;
        };

        void Decode(uint8_t command, const std::vector<uint8_t>& parameters);
        Span DecodeSpan(const std::vector<uint8_t>& parameters, uint16_t extent) const;
        void StartMemoryWrite();
        void WritePixels(const std::vector<uint8_t>& pixels);
        void WritePixel(const uint8_t* pixel);

    private:
        infra::ByteRange storage;
        DisplaySize size;
        PixelFormat format;
        Span columns;
        Span pages;
        uint16_t cursorX{ 0 };
        uint16_t cursorY{ 0 };
        bool writing{ false };
    };
}

#endif
