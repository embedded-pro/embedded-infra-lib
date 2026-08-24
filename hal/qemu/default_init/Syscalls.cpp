#include "hal/qemu/cortex/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"

// Strong definition overriding the weak _write in hal.cortex_m, routing stdout
// and stderr to the semihosting host.
extern "C" int _write(int, const char* buf, int count)
{
    hal::cortex::SemihostingWrite(infra::ConstByteRange(
        reinterpret_cast<const uint8_t*>(buf),
        reinterpret_cast<const uint8_t*>(buf) + count));
    return count;
}
