#include "services/util/NvmImage.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace
{
    class InMemoryRegion
        : public services::NvmRegion
    {
    public:
        void Write(infra::ConstByteRange data, infra::Function<void()> onDone) override
        {
            std::copy(data.begin(), data.end(), storage.begin());
            if (corruptByte)
                storage[*corruptByte] ^= 0xFF;
            onDone();
        }

        void Read(infra::ByteRange data, infra::Function<void()> onDone) override
        {
            std::copy(storage.begin(), storage.begin() + data.size(), data.begin());
            onDone();
        }

        void Erase(infra::Function<void()> onDone) override
        {
            storage.fill(0xFF);
            onDone();
        }

        std::array<uint8_t, 8> storage{};
        std::optional<std::size_t> corruptByte;
    };

    class NvmImageTest
        : public testing::Test
    {
    public:
        InMemoryRegion region;
        services::NvmImage<4> image{ region };
        std::optional<bool> verified;
        std::array<uint8_t, 4> readBack{};

        void Write(std::array<uint8_t, 4> contents)
        {
            std::copy(contents.begin(), contents.end(), image.Contents().begin());
            image.Write([this](bool result)
                {
                    verified = result;
                });
        }

        void CaptureReadBack(infra::ConstByteRange contents)
        {
            std::copy(contents.begin(), contents.end(), readBack.begin());
        }
    };
}

TEST_F(NvmImageTest, a_write_that_reads_back_unchanged_is_verified)
{
    Write({ 1, 2, 3, 4 });

    EXPECT_EQ(true, verified);
    EXPECT_EQ(1, region.storage[0]);
    EXPECT_EQ(4, region.storage[3]);
}

TEST_F(NvmImageTest, a_write_that_reads_back_differently_is_not_verified)
{
    region.corruptByte = 2;

    Write({ 1, 2, 3, 4 });

    EXPECT_EQ(false, verified);
}

TEST_F(NvmImageTest, read_delivers_the_stored_bytes)
{
    region.storage = { 9, 8, 7, 6, 5, 4, 3, 2 };

    image.Read([this](infra::ConstByteRange contents)
        {
            CaptureReadBack(contents);
        });

    EXPECT_EQ((std::array<uint8_t, 4>{ 9, 8, 7, 6 }), readBack);
}

TEST_F(NvmImageTest, erase_delivers_the_erased_bytes)
{
    region.storage = { 1, 1, 1, 1, 1, 1, 1, 1 };

    image.Erase([this](infra::ConstByteRange contents)
        {
            CaptureReadBack(contents);
        });

    EXPECT_EQ((std::array<uint8_t, 4>{ 0xFF, 0xFF, 0xFF, 0xFF }), readBack);
}
