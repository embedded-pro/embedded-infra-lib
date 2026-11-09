#include "hal/qemu/semihosting/SemihostingFileSystemCortex.hpp"
#include "hal/cortex_m/Semihosting.hpp"

namespace hal
{
    namespace
    {
        cortex::SemihostingOpenMode ToOpenMode(SemihostingFileSystem::Mode mode)
        {
            switch (mode)
            {
                case SemihostingFileSystem::Mode::readWriteBinary:
                    return cortex::SemihostingOpenMode::readWriteBinary;
                case SemihostingFileSystem::Mode::writeBinary:
                    return cortex::SemihostingOpenMode::writeBinary;
                case SemihostingFileSystem::Mode::readBinary:
                    break;
            }

            return cortex::SemihostingOpenMode::readBinary;
        }
    }

    std::optional<SemihostingFileSystem::Handle> SemihostingFileSystemCortex::Open(const char* path, Mode mode)
    {
        const auto handle = cortex::SemihostingOpenFile(path, ToOpenMode(mode));
        if (handle < 0)
            return std::nullopt;

        return handle;
    }

    void SemihostingFileSystemCortex::Close(Handle handle)
    {
        cortex::SemihostingCloseFile(handle);
    }

    bool SemihostingFileSystemCortex::Seek(Handle handle, uint32_t position)
    {
        return cortex::SemihostingSeekFile(handle, position) == 0;
    }

    bool SemihostingFileSystemCortex::Write(Handle handle, infra::ConstByteRange data)
    {
        return cortex::SemihostingWriteFile(handle, data) == 0;
    }

    std::size_t SemihostingFileSystemCortex::Read(Handle handle, infra::ByteRange buffer)
    {
        return buffer.size() - cortex::SemihostingReadFile(handle, buffer);
    }
}
