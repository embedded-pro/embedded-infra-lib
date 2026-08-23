#include "hal/qemu/sync/SemihostingWriter.hpp"
#include <limits>

namespace hal
{
    SemihostingWriter::SemihostingWriter(WriteAction action)
        : writeAction(action)
    {}

    void SemihostingWriter::Insert(infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy)
    {
        if (!range.empty())
            writeAction(range);
    }

    std::size_t SemihostingWriter::Available() const
    {
        return std::numeric_limits<std::size_t>::max();
    }
}
