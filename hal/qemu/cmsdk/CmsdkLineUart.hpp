#ifndef HAL_QEMU_CMSDK_CMSDK_LINE_UART_HPP
#define HAL_QEMU_CMSDK_CMSDK_LINE_UART_HPP

#include "hal/qemu/cmsdk/CmsdkUartRegisters.hpp"
#include "hal/qemu/cmsdk/LineAssembler.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hal
{
    class CmsdkLineUart
    {
    public:
        template<std::size_t MaxLineLength>
        using WithMaxLineLength = infra::WithStorage<CmsdkLineUart, std::array<char, MaxLineLength + 1>>;

        explicit CmsdkLineUart(infra::MemoryRange<char> line, uintptr_t base = cmsdkUart0BaseAddress);
        CmsdkLineUart(const CmsdkLineUart&) = delete;
        CmsdkLineUart& operator=(const CmsdkLineUart&) = delete;
        ~CmsdkLineUart() = default;

        const char* PollLine();
        void Write(std::string_view text) const;

    private:
        CmsdkUartRegisters& uart;
        LineAssembler assembler;
    };
}

#endif
