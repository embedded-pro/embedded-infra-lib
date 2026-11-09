#include "hal/qemu/semihosting/SemihostingEeprom.hpp"
#include "hal/qemu/semihosting/test_doubles/SemihostingFileSystemMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
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

    constexpr const char* path = "eeprom->bin";
    constexpr hal::SemihostingFileSystem::Handle handle = 5;

    MATCHER_P(ContentsAre, expected, "")
    {
        return std::vector<uint8_t>(arg.begin(), arg.end()) == expected;
    }

    class SemihostingEepromTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        SemihostingEepromTest()
        {
            EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::readBinary)).WillOnce(Return(handle));
            EXPECT_CALL(fileSystem, Read(handle, _)).WillOnce([](hal::SemihostingFileSystem::Handle, infra::ByteRange buffer)
                {
                    buffer[0] = 0x11;
                    buffer[1] = 0x22;
                    return std::size_t{ 2 };
                });
            EXPECT_CALL(fileSystem, Close(handle));
            eeprom.emplace(fileSystem, path);
        }

        void ExpectFlush(std::vector<uint8_t> contents)
        {
            EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(handle));
            EXPECT_CALL(fileSystem, Write(handle, ContentsAre(contents))).WillOnce(Return(true));
            EXPECT_CALL(fileSystem, Close(handle));
        }

        testing::StrictMock<hal::SemihostingFileSystemMock> fileSystem;
        std::optional<hal::SemihostingEeprom::WithStorage<4>> eeprom;
        testing::StrictMock<testing::MockFunction<void()>> onDone;
    };
}

TEST_F(SemihostingEepromTest, size_is_storage_size)
{
    EXPECT_EQ(4, eeprom->Size());
}

TEST_F(SemihostingEepromTest, construction_loads_contents_over_erased_storage)
{
    EXPECT_EQ((std::array<uint8_t, 4>{ 0x11, 0x22, 0xff, 0xff }), eeprom->Storage());
}

TEST_F(SemihostingEepromTest, construction_without_file_leaves_storage_erased)
{
    EXPECT_CALL(fileSystem, Open(StrEq("missing.bin"), Mode::readBinary)).WillOnce(Return(std::nullopt));

    hal::SemihostingEeprom::WithStorage<4> missing{ fileSystem, "missing.bin" };

    EXPECT_EQ((std::array<uint8_t, 4>{ 0xff, 0xff, 0xff, 0xff }), missing.Storage());
}

TEST_F(SemihostingEepromTest, read_buffer_completes_asynchronously)
{
    std::array<uint8_t, 2> buffer{};
    eeprom->ReadBuffer(infra::MakeRange(buffer), 1, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 2>{ 0x22, 0xff }), buffer);
}

TEST_F(SemihostingEepromTest, read_beyond_end_reads_only_available_bytes)
{
    std::array<uint8_t, 3> buffer{ 1, 2, 3 };
    eeprom->ReadBuffer(infra::MakeRange(buffer), 3, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 3>{ 0xff, 2, 3 }), buffer);
}

TEST_F(SemihostingEepromTest, write_buffer_flushes_whole_storage_and_completes_asynchronously)
{
    ExpectFlush({ 0x11, 0xaa, 0xbb, 0xff });
    std::array<uint8_t, 2> data{ 0xaa, 0xbb };
    eeprom->WriteBuffer(infra::MakeRange(data), 1, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
}

TEST_F(SemihostingEepromTest, write_beyond_end_is_truncated)
{
    ExpectFlush({ 0x11, 0x22, 0xff, 0xaa });
    std::array<uint8_t, 2> data{ 0xaa, 0xbb };
    eeprom->WriteBuffer(infra::MakeRange(data), 3, infra::emptyFunction);
}

TEST_F(SemihostingEepromTest, write_without_file_completes)
{
    EXPECT_CALL(fileSystem, Open(StrEq(path), Mode::writeBinary)).WillOnce(Return(std::nullopt));
    std::array<uint8_t, 1> data{ 0xaa };
    eeprom->WriteBuffer(infra::MakeRange(data), 0, [this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xaa, 0x22, 0xff, 0xff }), eeprom->Storage());
}

TEST_F(SemihostingEepromTest, erase_flushes_erased_storage_and_completes_asynchronously)
{
    ExpectFlush({ 0xff, 0xff, 0xff, 0xff });
    eeprom->Erase([this]()
        {
            onDone.Call();
        });

    EXPECT_CALL(onDone, Call());
    ExecuteAllActions();
}
