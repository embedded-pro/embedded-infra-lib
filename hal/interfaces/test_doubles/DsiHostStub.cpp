#include "hal/interfaces/test_doubles/DsiHostStub.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    namespace
    {
        constexpr uint8_t setColumnAddress = 0x2a;
        constexpr uint8_t setPageAddress = 0x2b;
        constexpr uint8_t writeMemoryStart = 0x2c;
        constexpr uint8_t writeMemoryContinue = 0x3c;
    }

    DsiHostStub::DsiHostStub(infra::ByteRange storage, DisplaySize size, PixelFormat format)
        : storage(storage)
        , size(size)
        , format(format)
        , columns{ 0, static_cast<uint16_t>(size.width - 1) }
        , pages{ 0, static_cast<uint16_t>(size.height - 1) }
    {
        really_assert(size.width != 0 && size.height != 0);
        really_assert(storage.size() >= std::size_t{ size.width } * size.height * BytesPerPixel(format));
        std::fill(this->storage.begin(), this->storage.end(), 0);

        ON_CALL(*this, WriteDcsMock).WillByDefault([this](uint8_t command, std::vector<uint8_t> parameters)
            {
                Decode(command, parameters);
            });
    }

    infra::ConstByteRange DsiHostStub::PixelAt(uint16_t x, uint16_t y) const
    {
        really_assert(x < size.width && y < size.height);

        std::size_t pixelBytes = BytesPerPixel(format);
        auto begin = storage.begin() + (std::size_t{ y } * size.width + x) * pixelBytes;
        return infra::ConstByteRange(begin, begin + pixelBytes);
    }

    void DsiHostStub::Decode(uint8_t command, const std::vector<uint8_t>& parameters)
    {
        switch (command)
        {
            case setColumnAddress:
                columns = DecodeSpan(parameters, size.width);
                break;
            case setPageAddress:
                pages = DecodeSpan(parameters, size.height);
                break;
            case writeMemoryStart:
                StartMemoryWrite();
                WritePixels(parameters);
                break;
            case writeMemoryContinue:
                really_assert(writing);
                WritePixels(parameters);
                break;
            default:
                break;
        }
    }

    DsiHostStub::Span DsiHostStub::DecodeSpan(const std::vector<uint8_t>& parameters, uint16_t extent) const
    {
        really_assert(parameters.size() == 4);

        Span span{ static_cast<uint16_t>(parameters[0] << 8 | parameters[1]), static_cast<uint16_t>(parameters[2] << 8 | parameters[3]) };
        really_assert(span.first <= span.last && span.last < extent);
        return span;
    }

    void DsiHostStub::StartMemoryWrite()
    {
        writing = true;
        cursorX = columns.first;
        cursorY = pages.first;
    }

    void DsiHostStub::WritePixels(const std::vector<uint8_t>& pixels)
    {
        std::size_t pixelBytes = BytesPerPixel(format);
        really_assert(pixels.size() % pixelBytes == 0);

        for (std::size_t offset = 0; offset != pixels.size(); offset += pixelBytes)
            WritePixel(pixels.data() + offset);
    }

    void DsiHostStub::WritePixel(const uint8_t* pixel)
    {
        really_assert(cursorY <= pages.last);

        std::size_t pixelBytes = BytesPerPixel(format);
        std::copy(pixel, pixel + pixelBytes, storage.begin() + (std::size_t{ cursorY } * size.width + cursorX) * pixelBytes);

        if (cursorX == columns.last)
        {
            cursorX = columns.first;
            ++cursorY;
        }
        else
            ++cursorX;
    }
}
