#include "hal/qemu/semihosting/SemihostingRecordFile.hpp"
#include <algorithm>

namespace hal
{
    SemihostingRecordFile::SemihostingRecordFile(infra::ByteRange buffer, SemihostingFileSystem& fileSystem, const char* path)
        : buffer(buffer)
        , fileSystem(fileSystem)
        , handle(fileSystem.Open(path, SemihostingFileSystem::Mode::writeBinary))
    {}

    void SemihostingRecordFile::Append(std::string_view text)
    {
        if (!handle)
            return;

        if (text.size() > buffer.size() - used)
            Flush();

        const auto length = std::min(text.size(), buffer.size());
        std::copy_n(text.begin(), length, buffer.begin() + used);
        used += length;
    }

    void SemihostingRecordFile::Flush()
    {
        if (!handle || used == 0)
            return;

        fileSystem.Write(*handle, infra::Head(buffer, used));
        used = 0;
    }
}
