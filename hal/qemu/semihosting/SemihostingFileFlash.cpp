#include "hal/qemu/semihosting/SemihostingFileFlash.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    SemihostingFileFlash::SemihostingFileFlash(SemihostingFileSystem& fileSystem, const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector)
        : services::SynchronousFlashMemory(storage, sizeOfEachSector)
        , fileSystem(fileSystem)
        , path(path)
        , storage(storage)
    {
        std::fill(storage.begin(), storage.end(), 0xff);
        if (!Load())
            StoreAll();
    }

    void SemihostingFileFlash::WriteBuffer(infra::ConstByteRange buffer, uint32_t address)
    {
        services::SynchronousFlashMemory::WriteBuffer(buffer, address);
        Store(address, static_cast<uint32_t>(buffer.size()));
    }

    void SemihostingFileFlash::EraseSectors(uint32_t beginIndex, uint32_t endIndex)
    {
        services::SynchronousFlashMemory::EraseSectors(beginIndex, endIndex);
        const auto begin = AddressOfSector(beginIndex);
        Store(begin, AddressOfSector(endIndex) - begin);
    }

    bool SemihostingFileFlash::Load()
    {
        const auto handle = fileSystem.Open(path, SemihostingFileSystem::Mode::readBinary);
        if (!handle)
            return false;

        fileSystem.Read(*handle, storage);
        fileSystem.Close(*handle);
        return true;
    }

    void SemihostingFileFlash::StoreAll() const
    {
        const auto handle = fileSystem.Open(path, SemihostingFileSystem::Mode::writeBinary);
        really_assert(handle != std::nullopt);
        really_assert(fileSystem.Write(*handle, storage));
        fileSystem.Close(*handle);
    }

    void SemihostingFileFlash::Store(uint32_t address, uint32_t length) const
    {
        if (length == 0)
            return;

        const auto handle = fileSystem.Open(path, SemihostingFileSystem::Mode::readWriteBinary);
        really_assert(handle != std::nullopt);
        really_assert(fileSystem.Seek(*handle, address));
        really_assert(fileSystem.Write(*handle, infra::Head(infra::DiscardHead(storage, address), length)));
        fileSystem.Close(*handle);
    }
}
