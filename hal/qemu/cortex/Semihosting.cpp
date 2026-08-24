#include "hal/qemu/cortex/Semihosting.hpp"

namespace hal::cortex
{
    uint32_t SemihostingCall(SemihostingOperation operation, void* parameter)
    {
        uint32_t result;
        __asm volatile(
            "mov r0, %[op]\n"
            "mov r1, %[param]\n"
            "bkpt 0xAB\n"
            "mov %[result], r0\n"
            : [result] "=r"(result)
            : [op] "r"(static_cast<uint32_t>(operation)), [param] "r"(parameter)
            : "r0", "r1", "memory");
        return result;
    }

    void SemihostingWriteC(char c)
    {
        SemihostingCall(SemihostingOperation::writeC, &c);
    }

    void SemihostingWrite0(const char* nullTerminated)
    {
        SemihostingCall(SemihostingOperation::write0, const_cast<char*>(nullTerminated));
    }

    void SemihostingWrite(infra::ConstByteRange data)
    {
        for (uint8_t byte : data)
            SemihostingWriteC(static_cast<char>(byte));
    }
}
