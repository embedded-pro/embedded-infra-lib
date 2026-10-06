#ifndef HAL_DISPLAY_STUB_HPP
#define HAL_DISPLAY_STUB_HPP

#include "hal/interfaces/test_doubles/DisplayMock.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace hal
{
    class DisplayStub
        : public DisplayMock
    {
    public:
        DisplayStub(infra::ByteRange storage, DisplaySize size, PixelFormat format);

        DisplaySize Size() const override;
        PixelFormat Format() const override;

        void CompleteWrite();
        bool WritePending() const;
        infra::ConstByteRange PixelAt(uint16_t x, uint16_t y) const;

        template<std::size_t StorageSize>
        using WithStorage = infra::WithStorage<DisplayStub, std::array<uint8_t, StorageSize>>;

    private:
        void StoreWrite(const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone);

    private:
        infra::ByteRange storage;
        DisplaySize size;
        PixelFormat format;
        infra::AutoResetFunction<void()> pendingCompletion;
    };
}

#endif
