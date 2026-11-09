#ifndef HAL_QEMU_SEMIHOSTING_RECORD_FILE_HPP
#define HAL_QEMU_SEMIHOSTING_RECORD_FILE_HPP

#include "hal/qemu/semihosting/SemihostingFileSystem.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace hal
{
    class SemihostingRecordFile
    {
    public:
        template<std::size_t BufferSize>
        using WithBuffer = infra::WithStorage<SemihostingRecordFile, std::array<uint8_t, BufferSize>>;

        SemihostingRecordFile(infra::ByteRange buffer, SemihostingFileSystem& fileSystem, const char* path);

        void Append(std::string_view text);
        void Flush();

    private:
        infra::ByteRange buffer;
        SemihostingFileSystem& fileSystem;
        std::optional<SemihostingFileSystem::Handle> handle;
        std::size_t used{ 0 };
    };
}

#endif
