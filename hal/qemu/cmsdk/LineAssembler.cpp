#include "hal/qemu/cmsdk/LineAssembler.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal
{
    namespace
    {
        bool IsEndOfLine(char c)
        {
            return c == '\n' || c == '\r';
        }
    }

    LineAssembler::LineAssembler(infra::MemoryRange<char> line)
        : line(line)
    {
        really_assert(!line.empty());
    }

    const char* LineAssembler::Receive(char c)
    {
        if (!IsEndOfLine(c))
        {
            if (length < line.size() - 1)
                line[length++] = c;

            return nullptr;
        }

        if (length == 0)
            return nullptr;

        line[length] = '\0';
        length = 0;
        return line.begin();
    }
}
