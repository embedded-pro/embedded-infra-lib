#include "hal/interfaces/test_doubles/DisplayStub.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    DisplayStub::DisplayStub(infra::ByteRange storage, DisplaySize size, PixelFormat format)
        : storage(storage)
        , size(size)
        , format(format)
    {
        really_assert(storage.size() >= std::size_t{ size.width } * size.height * BytesPerPixel(format));
        std::fill(this->storage.begin(), this->storage.end(), 0);

        ON_CALL(*this, WriteWithStride).WillByDefault([this](const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone)
            {
                StoreWrite(area, pixels, strideInBytes, onDone);
            });
    }

    DisplaySize DisplayStub::Size() const
    {
        return size;
    }

    PixelFormat DisplayStub::Format() const
    {
        return format;
    }

    void DisplayStub::CompleteWrite()
    {
        really_assert(WritePending());
        pendingCompletion();
    }

    bool DisplayStub::WritePending() const
    {
        return static_cast<bool>(pendingCompletion);
    }

    infra::ConstByteRange DisplayStub::PixelAt(uint16_t x, uint16_t y) const
    {
        really_assert(x < size.width && y < size.height);

        std::size_t pixelBytes = BytesPerPixel(format);
        auto begin = storage.begin() + (std::size_t{ y } * size.width + x) * pixelBytes;
        return infra::ConstByteRange(begin, begin + pixelBytes);
    }

    void DisplayStub::StoreWrite(const DisplayArea& area, infra::ConstByteRange pixels, std::size_t strideInBytes, const infra::Function<void()>& onDone)
    {
        really_assert(!WritePending());
        really_assert(IsValidDisplayWrite(size, format, area, pixels, strideInBytes));

        std::size_t pixelBytes = BytesPerPixel(format);
        std::size_t rowBytes = area.width * pixelBytes;

        for (std::size_t row = 0; row != area.height && rowBytes != 0; ++row)
        {
            auto source = pixels.begin() + row * strideInBytes;
            auto destination = storage.begin() + ((std::size_t{ area.y } + row) * size.width + area.x) * pixelBytes;
            std::copy(source, source + rowBytes, destination);
        }

        pendingCompletion = onDone;
    }
}
