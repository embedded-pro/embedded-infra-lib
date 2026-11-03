#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    SynchronousFlashMemory::SynchronousFlashMemory(infra::ByteRange memory, uint32_t sizeOfEachSector)
        : hal::SynchronousFlashHomogeneous(static_cast<uint32_t>(memory.size()) / sizeOfEachSector, sizeOfEachSector)
        , memory(memory)
    {
        really_assert(memory.size() % sizeOfEachSector == 0);
    }

    void SynchronousFlashMemory::WriteBuffer(infra::ConstByteRange buffer, uint32_t address)
    {
        really_assert(address <= memory.size() && buffer.size() <= memory.size() - address);
        const auto held = infra::DiscardHead(memory, address);
        std::ranges::transform(buffer, held, held.begin(), [](uint8_t written, uint8_t current)
            {
                return static_cast<uint8_t>(current & written);
            });
    }

    void SynchronousFlashMemory::ReadBuffer(infra::ByteRange buffer, uint32_t address)
    {
        really_assert(address <= memory.size() && buffer.size() <= memory.size() - address);
        std::copy_n(memory.begin() + address, buffer.size(), buffer.begin());
    }

    void SynchronousFlashMemory::EraseSectors(uint32_t beginIndex, uint32_t endIndex)
    {
        const auto begin = AddressOfSector(beginIndex);
        const auto end = AddressOfSector(endIndex);
        really_assert(begin <= end && end <= memory.size());
        std::fill(memory.begin() + begin, memory.begin() + end, 0xff);
    }
}
