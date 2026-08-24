#include "hal/qemu/cortex/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"

extern "C" int _write(int, const char* buf, int count)
{
    hal::cortex::SemihostingWrite(infra::ConstByteRange(
        reinterpret_cast<const uint8_t*>(buf),
        reinterpret_cast<const uint8_t*>(buf) + count));
    return count;
}
