#include "hal/qemu/semihosting/SemihostingRecordFile.hpp"
#include "hal/qemu/semihosting/test_doubles/SemihostingFileSystemMock.hpp"
#include "infra/util/ByteRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>
#include <string>

namespace
{
    using Mode = hal::SemihostingFileSystem::Mode;
    using testing::Return;
    using testing::StrEq;

    constexpr const char* path = "plant.rec";
    constexpr hal::SemihostingFileSystem::Handle handle = 9;

    MATCHER_P(TextIs, expected, "")
    {
        return std::string(reinterpret_cast<const char*>(arg.begin()), arg.size()) == expected;
    }

    class SemihostingRecordFileTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::SemihostingFileSystemMock> fileSystem;
    };
}

TEST_F(SemihostingRecordFileTest, flush_writes_appended_text)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    hal::SemihostingRecordFile::WithBuffer<16> record{ fileSystem, path };

    record.Append("ab");
    record.Append("cd");

    EXPECT_CALL(fileSystem, Write(handle, TextIs("abcd"))).WillOnce(Return(true));
    record.Flush();
}

TEST_F(SemihostingRecordFileTest, flush_without_pending_text_does_not_write)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    hal::SemihostingRecordFile::WithBuffer<16> record{ fileSystem, path };

    record.Flush();
}

TEST_F(SemihostingRecordFileTest, flush_empties_buffer)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    hal::SemihostingRecordFile::WithBuffer<16> record{ fileSystem, path };

    record.Append("ab");
    EXPECT_CALL(fileSystem, Write(handle, TextIs("ab"))).WillOnce(Return(true));
    record.Flush();

    record.Append("cd");
    EXPECT_CALL(fileSystem, Write(handle, TextIs("cd"))).WillOnce(Return(true));
    record.Flush();
}

TEST_F(SemihostingRecordFileTest, append_flushes_when_text_does_not_fit)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    hal::SemihostingRecordFile::WithBuffer<4> record{ fileSystem, path };

    record.Append("abc");
    EXPECT_CALL(fileSystem, Write(handle, TextIs("abc"))).WillOnce(Return(true));
    record.Append("de");

    EXPECT_CALL(fileSystem, Write(handle, TextIs("de"))).WillOnce(Return(true));
    record.Flush();
}

TEST_F(SemihostingRecordFileTest, text_longer_than_buffer_is_truncated)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    hal::SemihostingRecordFile::WithBuffer<4> record{ fileSystem, path };

    record.Append("abcdef");

    EXPECT_CALL(fileSystem, Write(handle, TextIs("abcd"))).WillOnce(Return(true));
    record.Flush();
}

TEST_F(SemihostingRecordFileTest, without_file_append_and_flush_do_nothing)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(std::nullopt));
    hal::SemihostingRecordFile::WithBuffer<4> record{ fileSystem, path };

    record.Append("ab");
    record.Flush();
}
