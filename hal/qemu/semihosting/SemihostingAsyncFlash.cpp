#include "hal/qemu/semihosting/SemihostingAsyncFlash.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace hal
{
    SemihostingAsyncFlash::SemihostingAsyncFlash(SemihostingFileSystem& fileSystem, const char* path, infra::ByteRange storage, uint32_t sizeOfEachSector)
        : FlashHomogeneous(static_cast<uint32_t>(storage.size()) / sizeOfEachSector, sizeOfEachSector)
        , file(fileSystem, path, storage, sizeOfEachSector)
    {}

    void SemihostingAsyncFlash::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        file.WriteBuffer(buffer, address);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void SemihostingAsyncFlash::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        file.ReadBuffer(buffer, address);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void SemihostingAsyncFlash::EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone)
    {
        file.EraseSectors(beginIndex, endIndex);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }
}
