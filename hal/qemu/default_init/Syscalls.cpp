#include "hal/cortex_m/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" int _write(int, const char* buf, int count)
{
    hal::cortex::SemihostingWrite(infra::ConstByteRange(
        reinterpret_cast<const uint8_t*>(buf),
        reinterpret_cast<const uint8_t*>(buf) + count));
    return count;
}

extern "C" [[noreturn]] void abort()
{
    static uint32_t exitBlock[2] = { 0x20026u, 1u };
    hal::cortex::SemihostingCall(hal::cortex::SemihostingOperation::exitExtended, exitBlock);

    while (true)
    {}
}
