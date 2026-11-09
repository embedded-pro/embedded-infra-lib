#ifndef HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_CORTEX_HPP
#define HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_CORTEX_HPP

#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"

namespace hal
{
    class SemihostingFileSystemCortex
        : public SemihostingFileSystem
    {
    public:
        std::optional<Handle> Open(const char* path, Mode mode) override;
        void Close(Handle handle) override;
        bool Seek(Handle handle, uint32_t position) override;
        bool Write(Handle handle, infra::ConstByteRange data) override;
        std::size_t Read(Handle handle, infra::ByteRange buffer) override;
    };
}

#endif
