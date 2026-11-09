#include "hal/cortex_m/Semihosting.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace
{
    uint32_t AddressOf(const void* pointer)
    {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
    }
}

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

    int32_t SemihostingOpenFile(const char* path, SemihostingOpenMode mode)
    {
        std::array<uint32_t, 3> parameters{ AddressOf(path), static_cast<uint32_t>(mode), static_cast<uint32_t>(std::strlen(path)) };
        return static_cast<int32_t>(SemihostingCall(SemihostingOperation::open, parameters.data()));
    }

    int32_t SemihostingCloseFile(int32_t handle)
    {
        auto parameter = static_cast<uint32_t>(handle);
        return static_cast<int32_t>(SemihostingCall(SemihostingOperation::close, &parameter));
    }

    int32_t SemihostingSeekFile(int32_t handle, uint32_t position)
    {
        std::array<uint32_t, 2> parameters{ static_cast<uint32_t>(handle), position };
        return static_cast<int32_t>(SemihostingCall(SemihostingOperation::seek, parameters.data()));
    }

    uint32_t SemihostingWriteFile(int32_t handle, infra::ConstByteRange data)
    {
        std::array<uint32_t, 3> parameters{ static_cast<uint32_t>(handle), AddressOf(data.begin()), static_cast<uint32_t>(data.size()) };
        return SemihostingCall(SemihostingOperation::write, parameters.data());
    }

    uint32_t SemihostingReadFile(int32_t handle, infra::ByteRange buffer)
    {
        std::array<uint32_t, 3> parameters{ static_cast<uint32_t>(handle), AddressOf(buffer.begin()), static_cast<uint32_t>(buffer.size()) };
        return SemihostingCall(SemihostingOperation::read, parameters.data());
    }
}
