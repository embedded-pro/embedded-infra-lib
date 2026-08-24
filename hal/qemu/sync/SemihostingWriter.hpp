#ifndef HAL_QEMU_SYNC_SEMIHOSTING_WRITER_HPP
#define HAL_QEMU_SYNC_SEMIHOSTING_WRITER_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"

namespace hal
{
    class SemihostingWriter
        : public infra::StreamWriter
    {
    public:
        using WriteAction = infra::Function<void(infra::ConstByteRange)>;

        explicit SemihostingWriter(WriteAction action);

        void Insert(infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy) override;
        std::size_t Available() const override;

    private:
        WriteAction writeAction;
    };
}

#endif
