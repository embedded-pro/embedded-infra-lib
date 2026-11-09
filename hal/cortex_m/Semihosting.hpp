#ifndef HAL_CORTEX_M_SEMIHOSTING_HPP
#define HAL_CORTEX_M_SEMIHOSTING_HPP

#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace hal::cortex
{
    enum class SemihostingOperation : uint32_t
    {
        open = 0x01,
        close = 0x02,
        writeC = 0x03,
        write0 = 0x04,
        write = 0x05,
        read = 0x06,
        seek = 0x0a,
        exit = 0x18,
        exitExtended = 0x20,
    };

    enum class SemihostingOpenMode : uint32_t
    {
        readBinary = 1,
        readWriteBinary = 3,
        writeBinary = 5,
    };

    uint32_t SemihostingCall(SemihostingOperation operation, void* parameter);
    void SemihostingWriteC(char c);
    void SemihostingWrite0(const char* nullTerminated);
    void SemihostingWrite(infra::ConstByteRange data);

    int32_t SemihostingOpenFile(const char* path, SemihostingOpenMode mode);
    int32_t SemihostingCloseFile(int32_t handle);
    int32_t SemihostingSeekFile(int32_t handle, uint32_t position);
    uint32_t SemihostingWriteFile(int32_t handle, infra::ConstByteRange data);
    uint32_t SemihostingReadFile(int32_t handle, infra::ByteRange buffer);
}

#endif
