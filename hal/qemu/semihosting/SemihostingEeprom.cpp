#include "hal/qemu/semihosting/SemihostingEeprom.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <algorithm>

namespace hal
{
    SemihostingEeprom::SemihostingEeprom(infra::ByteRange storage, SemihostingFileSystem& fileSystem, const char* path)
        : storage(storage)
        , fileSystem(fileSystem)
        , path(path)
    {
        std::fill(storage.begin(), storage.end(), 0xff);
        LoadFromFile();
    }

    uint32_t SemihostingEeprom::Size() const
    {
        return static_cast<uint32_t>(storage.size());
    }

    void SemihostingEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        auto destination = infra::DiscardHead(storage, address);
        infra::Copy(infra::Head(buffer, destination.size()), infra::Head(destination, buffer.size()));
        FlushToFile();
        Complete(onDone);
    }

    void SemihostingEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        auto source = infra::DiscardHead(storage, address);
        infra::Copy(infra::Head(source, buffer.size()), infra::Head(buffer, source.size()));
        Complete(onDone);
    }

    void SemihostingEeprom::Erase(infra::Function<void()> onDone)
    {
        std::fill(storage.begin(), storage.end(), 0xff);
        FlushToFile();
        Complete(onDone);
    }

    void SemihostingEeprom::LoadFromFile()
    {
        const auto handle = fileSystem.Open(path, SemihostingFileSystem::Mode::readBinary);
        if (!handle)
            return;

        fileSystem.Read(*handle, storage);
        fileSystem.Close(*handle);
    }

    void SemihostingEeprom::FlushToFile() const
    {
        const auto handle = fileSystem.Open(path, SemihostingFileSystem::Mode::writeBinary);
        if (!handle)
            return;

        fileSystem.Write(*handle, storage);
        fileSystem.Close(*handle);
    }

    void SemihostingEeprom::Complete(const infra::Function<void()>& onDone) const
    {
        if (onDone)
            infra::EventDispatcher::Instance().Schedule(onDone);
    }
}
