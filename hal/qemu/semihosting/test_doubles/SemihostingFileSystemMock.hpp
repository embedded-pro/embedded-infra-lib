#ifndef HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_MOCK_HPP
#define HAL_QEMU_SEMIHOSTING_FILE_SYSTEM_MOCK_HPP

#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SemihostingFileSystemMock
        : public SemihostingFileSystem
    {
    public:
        MOCK_METHOD(std::optional<Handle>, Open, (const char* path, Mode mode), (override));
        MOCK_METHOD(void, Close, (Handle handle), (override));
        MOCK_METHOD(bool, Seek, (Handle handle, uint32_t position), (override));
        MOCK_METHOD(bool, Write, (Handle handle, infra::ConstByteRange data), (override));
        MOCK_METHOD(std::size_t, Read, (Handle handle, infra::ByteRange buffer), (override));
    };
}

#endif
