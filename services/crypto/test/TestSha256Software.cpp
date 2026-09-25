#include "services/crypto/Sha256Software.hpp"
#include "gtest/gtest.h"
#include <string>

namespace
{
    std::string Hex(const services::Sha256::Digest& digest)
    {
        static const char* digits = "0123456789abcdef";
        std::string result;

        for (auto byte : digest)
        {
            result += digits[byte >> 4];
            result += digits[byte & 0x0f];
        }

        return result;
    }

    std::string HashOf(const std::string& message)
    {
        services::Sha256Software sha256;
        return Hex(sha256.Calculate(infra::ConstByteRange(reinterpret_cast<const uint8_t*>(message.data()), reinterpret_cast<const uint8_t*>(message.data() + message.size()))));
    }
}

TEST(Sha256SoftwareTest, hashes_the_empty_message)
{
    EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", HashOf(""));
}

TEST(Sha256SoftwareTest, hashes_a_message_shorter_than_a_block)
{
    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", HashOf("abc"));
}

TEST(Sha256SoftwareTest, hashes_a_message_whose_padding_spills_into_a_second_block)
{
    EXPECT_EQ("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", HashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

TEST(Sha256SoftwareTest, hashes_a_message_longer_than_a_block)
{
    EXPECT_EQ("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1", HashOf("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"));
}

TEST(Sha256SoftwareTest, hashes_a_million_characters)
{
    EXPECT_EQ("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", HashOf(std::string(1000000, 'a')));
}
