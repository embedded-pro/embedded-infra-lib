#ifndef HAL_QEMU_CORTEX_SEMIHOSTING_HPP
#define HAL_QEMU_CORTEX_SEMIHOSTING_HPP

#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace hal::cortex
{
    enum class SemihostingOperation : uint32_t
    {
        writeC = 0x03,
        write0 = 0x04,
        write = 0x05,
        exit = 0x18,
    };

    uint32_t SemihostingCall(SemihostingOperation operation, void* parameter);
    void SemihostingWriteC(char c);
    void SemihostingWrite0(const char* nullTerminated);
    void SemihostingWrite(infra::ConstByteRange data);
}

#endif
