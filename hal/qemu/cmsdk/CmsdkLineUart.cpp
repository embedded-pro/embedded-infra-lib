#include "hal/qemu/cmsdk/CmsdkLineUart.hpp"

namespace hal
{
    CmsdkLineUart::CmsdkLineUart(infra::MemoryRange<char> line, uintptr_t base)
        : uart(*reinterpret_cast<CmsdkUartRegisters*>(base))
        , assembler(line)
    {
        uart.control = uartControlTxEnable | uartControlRxEnable;
    }

    const char* CmsdkLineUart::PollLine()
    {
        while ((uart.state & uartStateRxFull) != 0)
        {
            if (const auto* line = assembler.Receive(static_cast<char>(uart.data & uartDataMask)); line != nullptr)
                return line;
        }

        return nullptr;
    }

    void CmsdkLineUart::Write(std::string_view text) const
    {
        for (const auto c : text)
        {
            while ((uart.state & uartStateTxFull) != 0)
            {}

            uart.data = static_cast<uint8_t>(c);
        }
    }
}
