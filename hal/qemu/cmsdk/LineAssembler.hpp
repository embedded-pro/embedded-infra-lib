#ifndef HAL_QEMU_CMSDK_LINE_ASSEMBLER_HPP
#define HAL_QEMU_CMSDK_LINE_ASSEMBLER_HPP

#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>

namespace hal
{
    class LineAssembler
    {
    public:
        template<std::size_t MaxLineLength>
        using WithMaxLineLength = infra::WithStorage<LineAssembler, std::array<char, MaxLineLength + 1>>;

        explicit LineAssembler(infra::MemoryRange<char> line);
        LineAssembler(const LineAssembler&) = delete;
        LineAssembler& operator=(const LineAssembler&) = delete;
        ~LineAssembler() = default;

        const char* Receive(char c);

    private:
        infra::MemoryRange<char> line;
        std::size_t length{ 0 };
    };
}

#endif
