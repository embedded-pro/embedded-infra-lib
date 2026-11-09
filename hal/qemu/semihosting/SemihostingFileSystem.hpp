#ifndef HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_HPP
#define HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_HPP

#include "infra/util/ByteRange.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace hal
{
    class SemihostingFileSystem
    {
    protected:
        SemihostingFileSystem() = default;
        SemihostingFileSystem(const SemihostingFileSystem& other) = delete;
        SemihostingFileSystem& operator=(const SemihostingFileSystem& other) = delete;
        ~SemihostingFileSystem() = default;

    public:
        enum class Mode : uint8_t
        {
            readBinary,
            readWriteBinary,
            writeBinary
        };

        using Handle = int32_t;

        virtual std::optional<Handle> Open(const char* path, Mode mode) = 0;
        virtual void Close(Handle handle) = 0;
        virtual bool Seek(Handle handle, uint32_t position) = 0;
        virtual bool Write(Handle handle, infra::ConstByteRange data) = 0;
        virtual std::size_t Read(Handle handle, infra::ByteRange buffer) = 0;
    };
}

#endif
