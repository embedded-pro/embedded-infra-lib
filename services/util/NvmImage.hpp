#ifndef SERVICES_NVM_IMAGE_HPP
#define SERVICES_NVM_IMAGE_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/util/NvmRegion.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    template<std::size_t Size>
    class NvmImage
    {
    public:
        explicit NvmImage(NvmRegion& region)
            : region(region)
        {}

        infra::ByteRange Contents()
        {
            return infra::MakeRange(written);
        }

        void Write(const infra::Function<void(bool verified)>& onDone)
        {
            onWritten = onDone;
            region.Write(infra::MakeConstByteRange(written), [this]
                {
                    region.Read(infra::MakeRange(readBack), [this]
                        {
                            onWritten(written == readBack);
                        });
                });
        }

        void Read(const infra::Function<void(infra::ConstByteRange image)>& onDone)
        {
            onRead = onDone;
            ReadBack();
        }

        void Erase(const infra::Function<void(infra::ConstByteRange image)>& onDone)
        {
            onRead = onDone;
            region.Erase([this]
                {
                    ReadBack();
                });
        }

    private:
        void ReadBack()
        {
            region.Read(infra::MakeRange(readBack), [this]
                {
                    onRead(infra::MakeConstByteRange(readBack));
                });
        }

        NvmRegion& region;
        std::array<uint8_t, Size> written{};
        std::array<uint8_t, Size> readBack{};
        infra::AutoResetFunction<void(bool)> onWritten;
        infra::AutoResetFunction<void(infra::ConstByteRange)> onRead;
    };
}

#endif
