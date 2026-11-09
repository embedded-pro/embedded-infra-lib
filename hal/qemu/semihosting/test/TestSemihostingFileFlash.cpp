#include "hal/qemu/semihosting/SemihostingFileFlash.hpp"
#include "hal/qemu/semihosting/test_doubles/SemihostingFileSystemMock.hpp"
#include "infra/util/ByteRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Mode = hal::SemihostingFileSystem::Mode;
    using testing::_;
    using testing::Return;
    using testing::StrEq;

    constexpr const char* path = "flash.bin";
    constexpr hal::SemihostingFileSystem::Handle handle = 7;

    MATCHER_P(ContentsAre, expected, "")
    {
        return std::vector<uint8_t>(arg.begin(), arg.end()) == expected;
    }

    class SemihostingFileFlashTest
        : public testing::Test
    {
    public:
        void ExpectLoad(std::vector<uint8_t> contents)
        {
            EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::readBinary)).WillOnce(Return(handle));
            EXPECT_CALL(fileSystem, Read(handle, _)).WillOnce([contents](hal::SemihostingFileSystem::Handle, infra::ByteRange buffer)
                {
                    std::copy(contents.begin(), contents.end(), buffer.begin());
                    return contents.size();
                });
            EXPECT_CALL(fileSystem, Close(handle));
        }

        void ExpectStore(uint32_t address, std::vector<uint8_t> contents)
        {
            EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::readWriteBinary)).WillOnce(Return(handle));
            EXPECT_CALL(fileSystem, Seek(handle, address)).WillOnce(Return(true));
            EXPECT_CALL(fileSystem, Write(handle, ContentsAre(contents))).WillOnce(Return(true));
            EXPECT_CALL(fileSystem, Close(handle));
        }

        testing::StrictMock<hal::SemihostingFileSystemMock> fileSystem;
        std::array<uint8_t, 8> storage{};
    };
}

TEST_F(SemihostingFileFlashTest, construction_loads_contents_from_existing_file)
{
    ExpectLoad({ 1, 2, 3 });

    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    EXPECT_EQ((std::array<uint8_t, 8>{ 1, 2, 3, 0xff, 0xff, 0xff, 0xff, 0xff }), storage);
    EXPECT_EQ(2, flash.NumberOfSectors());
    EXPECT_EQ(4, flash.SizeOfSector(0));
}

TEST_F(SemihostingFileFlashTest, construction_creates_erased_file_when_none_exists)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::readBinary)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
    EXPECT_CALL(fileSystem, Write(handle, ContentsAre(std::vector<uint8_t>(8, 0xff)))).WillOnce(Return(true));
    EXPECT_CALL(fileSystem, Close(handle));

    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    EXPECT_EQ((std::array<uint8_t, 8>{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }), storage);
}

TEST_F(SemihostingFileFlashTest, write_buffer_stores_written_bytes_at_their_address)
{
    ExpectLoad(std::vector<uint8_t>(8, 0xff));
    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    ExpectStore(2, { 0x12, 0x34 });
    std::array<uint8_t, 2> data{ 0x12, 0x34 };
    flash.WriteBuffer(infra::MakeRange(data), 2);

    std::array<uint8_t, 2> readBack{};
    flash.ReadBuffer(infra::MakeRange(readBack), 2);
    EXPECT_EQ(data, readBack);
}

TEST_F(SemihostingFileFlashTest, write_of_empty_buffer_does_not_touch_file)
{
    ExpectLoad(std::vector<uint8_t>(8, 0xff));
    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    flash.WriteBuffer(infra::ConstByteRange(), 3);
}

TEST_F(SemihostingFileFlashTest, erase_sectors_stores_erased_sectors)
{
    ExpectLoad({ 0, 1, 2, 3, 4, 5, 6, 7 });
    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    ExpectStore(4, { 0xff, 0xff, 0xff, 0xff });
    flash.EraseSectors(1, 2);

    EXPECT_EQ((std::array<uint8_t, 8>{ 0, 1, 2, 3, 0xff, 0xff, 0xff, 0xff }), storage);
}

TEST_F(SemihostingFileFlashTest, erase_of_no_sectors_does_not_touch_file)
{
    ExpectLoad(std::vector<uint8_t>(8, 0xff));
    hal::SemihostingFileFlash flash{ fileSystem, path, infra::MakeRange(storage), 4 };

    flash.EraseSectors(1, 1);
}
