#include "hal/interfaces/test_doubles/FlashStub.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <algorithm>

namespace hal
{
    template<class T>
    FlashStubBase<T>::FlashStubBase(T numberOfSectors, uint32_t sizeOfEachSector)
        : sectors(static_cast<std::size_t>(numberOfSectors), std::vector<uint8_t>(sizeOfEachSector, 0xff))
    {}

    template<class T>
    T FlashStubBase<T>::NumberOfSectors() const
    {
        return sectors.size();
    }

    template<class T>
    uint32_t FlashStubBase<T>::SizeOfSector(T sectorIndex) const
    {
        return sectors[static_cast<std::size_t>(sectorIndex)].size();
    }

    template<class T>
    T FlashStubBase<T>::SectorOfAddress(T address) const
    {
        for (std::size_t index = 0; index != sectors.size(); ++index)
        {
            if (address < sectors[index].size())
                return index;

            address -= sectors[index].size();
        }

        assert(address == 0);
        return sectors.size();
    }

    template<class T>
    T FlashStubBase<T>::AddressOfSector(T sectorIndex) const
    {
        assert(sectorIndex <= sectors.size());

        uint32_t result = 0;

        for (uint32_t index = 0; index != sectorIndex; ++index)
            result += sectors[index].size();

        return result;
    }

    template<class T>
    void FlashStubBase<T>::WriteBuffer(infra::ConstByteRange buffer, T address, infra::Function<void()> onDone)
    {
        infra::EventDispatcher::Instance().Schedule(onDone);

        if (stopAfterWriteSteps)
        {
            if (*stopAfterWriteSteps > 0)
                --*stopAfterWriteSteps;
            else
                return;
        }

        auto [index, offset] = Locate(address);
        while (!buffer.empty())
        {
            auto& sector = sectors[index];
            std::size_t size = std::min<std::size_t>(buffer.size(), sector.size() - offset);

            for (std::size_t i = 0; i != size; ++i)
                sector[offset + i] &= buffer[i];

            buffer.pop_front(size);
            ++index;
            offset = 0;
        }
    }

    template<class T>
    void FlashStubBase<T>::ReadBuffer(infra::ByteRange buffer, T address, infra::Function<void()> onDone)
    {
        auto [index, offset] = Locate(address);
        while (!buffer.empty())
        {
            const auto& sector = sectors[index];
            std::size_t size = std::min<std::size_t>(buffer.size(), sector.size() - offset);

            std::copy(sector.begin() + offset, sector.begin() + offset + size, buffer.begin());

            buffer.pop_front(size);
            index = (index + 1) % sectors.size();
            offset = 0;
        }

        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    template<class T>
    void FlashStubBase<T>::EraseSectors(T beginIndex, T endIndex, infra::Function<void()> onDone)
    {
        for (T sectorIndex = beginIndex; sectorIndex != endIndex; ++sectorIndex)
            std::fill(sectors[static_cast<std::size_t>(sectorIndex)].begin(), sectors[static_cast<std::size_t>(sectorIndex)].end(), 0xff);

        onEraseDone = onDone;
        if (!delaySignalEraseDone)
            infra::EventDispatcher::Instance().Schedule(onDone);
    }

    template<class T>
    std::pair<std::size_t, std::size_t> FlashStubBase<T>::Locate(T address) const
    {
        std::size_t index = 0;
        while (index != sectors.size() && address >= sectors[index].size())
        {
            address -= sectors[index].size();
            ++index;
        }

        return { index, static_cast<std::size_t>(address) };
    }

    template class FlashStubBase<uint32_t>;
    template class FlashStubBase<uint64_t>;
}
