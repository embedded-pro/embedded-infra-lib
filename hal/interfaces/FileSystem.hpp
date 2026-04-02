#ifndef HAL_INTERFACE_FILE_SYSTEM_HPP
#define HAL_INTERFACE_FILE_SYSTEM_HPP

#include <cstdint>
#include <string>
#include <vector>

// Detect proper filesystem header and namespace; first by using
// feature testing macros, then by using include file testing.
#include <filesystem>

namespace hal
{
    namespace filesystem = std::filesystem;
}

#include "infra/util/ByteRange.hpp"

namespace hal
{
    struct CannotOpenFileException
        : std::runtime_error
    {
        explicit CannotOpenFileException(const hal::filesystem::path& path);
    };

    struct EmptyFileException
        : std::runtime_error
    {
        explicit EmptyFileException(const hal::filesystem::path& path);
    };

    class FileSystem
    {
    public:
        virtual std::vector<std::string> ReadFile(const hal::filesystem::path& path) = 0;
        virtual void WriteFile(const hal::filesystem::path& path, const std::vector<std::string>& contents) = 0;

        virtual std::vector<uint8_t> ReadBinaryFile(const hal::filesystem::path& path) = 0;
        virtual void WriteBinaryFile(const hal::filesystem::path& path, infra::ConstByteRange contents) = 0;

    protected:
        ~FileSystem() = default;
    };
}

#endif
