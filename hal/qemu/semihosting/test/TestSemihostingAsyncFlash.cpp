#include "hal/qemu/semihosting/SemihostingAsyncFlash.hpp"
#include "hal/qemu/semihosting/test_doubles/SemihostingFileSystemMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
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

    constexpr const char* path = "staging.bin";
    constexpr hal::SemihostingFileSystem::Handle handle = 3;

    MATCHER_P(ContentsAre, expected, "")
    {
        return std::vector<uint8_t>(arg.begin(), arg.end()) == expected;
    }

    class SemihostingAsyncFlashTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        SemihostingAsyncFlashTest()
        {
            EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::readBinary)).WillOnce(Return(handle));
            EXPECT_CALL(fileSystem, Read(handle, _)).WillOnce([](hal::SemihostingFileSystem::Handle, infra::ByteRange buffer)
                {
                    for (std::size_t i = 0; i != buffer.size(); ++i)
                        buffer[i] = static_cast<uint8_t>(i);
                    return buffer.size();
                });
            EXPECT_CALL(fileSystem, Close(handle));
            flash.emplace(fileSystem, path, infra::MakeRange(storage), 4);
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
        std::optional<hal::SemihostingAsyncFlash> flash;
        testing::StrictMock<testing::MockFunction<void()>> onDone;
    };
}

TEST_F(SemihostingAsyncFlashTest, has_homogeneous_sectors_over_storage)
{
    EXPECT_EQ(2, flash->NumberOfSectors());
    EXPECT_EQ(4, flash->SizeOfSector(1));
}

TEST_F(SemihostingAsyncFlashTest, read_buffer_completes_asynchronously)
{
    std::array<uint8_t, 3> buffer{};
    flash->ReadBuffer(infra::MakeRange(buffer), 5, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 3>{ 5, 6, 7 }), buffer);
}

TEST_F(SemihostingAsyncFlashTest, write_buffer_stores_and_completes_asynchronously)
{
    ExpectStore(1, { 0, 0 });
    std::array<uint8_t, 2> data{ 0, 0 };
    flash->WriteBuffer(infra::MakeRange(data), 1, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
}

TEST_F(SemihostingAsyncFlashTest, erase_sectors_stores_and_completes_asynchronously)
{
    ExpectStore(0, { 0xff, 0xff, 0xff, 0xff });
    flash->EraseSectors(0, 1, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 8>{ 0xff, 0xff, 0xff, 0xff, 4, 5, 6, 7 }), storage);
}
