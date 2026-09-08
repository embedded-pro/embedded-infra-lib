#include "hal/cortex_m/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace
{
    constexpr std::array<char, 16> hexDigits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::array<char, 24> FormatAbortMessage(uint32_t lr)
    {
        std::array<char, 24> msg{};
        std::size_t pos = 0;

        for (char c : std::array<char, 13>{
                 'A', 'B', 'O', 'R', 'T', ' ', '@', ' ', 'L', 'R', '=', '0', 'x' })
            msg[pos++] = c;

        for (int shift = 28; shift >= 0; shift -= 4)
            msg[pos++] = hexDigits[(lr >> shift) & 0xfu];

        msg[pos++] = '\n';
        msg[pos] = '\0';

        return msg;
    }
}

extern "C" int _write(int, const char* buf, int count)
{
    hal::cortex::SemihostingWrite(infra::ConstByteRange(
        reinterpret_cast<const uint8_t*>(buf),
        reinterpret_cast<const uint8_t*>(buf) + count));
    return count;
}

extern "C" void abort()
{
    volatile uint32_t lr = 0;
    asm volatile("mov %0, lr" : "=r"(lr));

    const auto msg = FormatAbortMessage(lr);
    hal::cortex::SemihostingWrite0(msg.data());

    static std::array<uint32_t, 2> exitBlock{ 0x20026u, 1u };
    hal::cortex::SemihostingCall(hal::cortex::SemihostingOperation::exitExtended, exitBlock.data());

    while (true)
    {}
}
