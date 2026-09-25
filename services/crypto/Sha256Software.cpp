#include "services/crypto/Sha256Software.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr std::size_t blockSize = 64;
        constexpr std::size_t lengthSize = 8;

        constexpr std::array<uint32_t, 64> roundConstants{ {
            0x428a2f98,
            0x71374491,
            0xb5c0fbcf,
            0xe9b5dba5,
            0x3956c25b,
            0x59f111f1,
            0x923f82a4,
            0xab1c5ed5,
            0xd807aa98,
            0x12835b01,
            0x243185be,
            0x550c7dc3,
            0x72be5d74,
            0x80deb1fe,
            0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1,
            0xefbe4786,
            0x0fc19dc6,
            0x240ca1cc,
            0x2de92c6f,
            0x4a7484aa,
            0x5cb0a9dc,
            0x76f988da,
            0x983e5152,
            0xa831c66d,
            0xb00327c8,
            0xbf597fc7,
            0xc6e00bf3,
            0xd5a79147,
            0x06ca6351,
            0x14292967,
            0x27b70a85,
            0x2e1b2138,
            0x4d2c6dfc,
            0x53380d13,
            0x650a7354,
            0x766a0abb,
            0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1,
            0xa81a664b,
            0xc24b8b70,
            0xc76c51a3,
            0xd192e819,
            0xd6990624,
            0xf40e3585,
            0x106aa070,
            0x19a4c116,
            0x1e376c08,
            0x2748774c,
            0x34b0bcb5,
            0x391c0cb3,
            0x4ed8aa4a,
            0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee,
            0x78a5636f,
            0x84c87814,
            0x8cc70208,
            0x90befffa,
            0xa4506ceb,
            0xbef9a3f7,
            0xc67178f2,
        } };

        constexpr std::array<uint32_t, 8> initialState{ {
            0x6a09e667,
            0xbb67ae85,
            0x3c6ef372,
            0xa54ff53a,
            0x510e527f,
            0x9b05688c,
            0x1f83d9ab,
            0x5be0cd19,
        } };

        constexpr uint32_t RotateRight(uint32_t value, uint32_t count)
        {
            return (value >> count) | (value << (32 - count));
        }

        uint32_t ReadBigEndian(const uint8_t* bytes)
        {
            return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) | (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
        }

        void WriteBigEndian(uint64_t value, uint8_t* bytes, std::size_t size)
        {
            for (std::size_t index = 0; index != size; ++index)
                bytes[size - 1 - index] = static_cast<uint8_t>(value >> (8 * index));
        }
    }

    Sha256::Digest Sha256Software::Calculate(infra::ConstByteRange input) const
    {
        State state = initialState;

        const auto completeBlocks = input.size() / blockSize;
        for (std::size_t block = 0; block != completeBlocks; ++block)
            Compress(state, input.begin() + block * blockSize);

        const auto remainder = input.size() % blockSize;
        std::array<uint8_t, 2 * blockSize> tail{};
        std::copy(input.begin() + completeBlocks * blockSize, input.end(), tail.begin());
        tail[remainder] = 0x80;

        const auto tailSize = remainder + 1 + lengthSize <= blockSize ? blockSize : 2 * blockSize;
        WriteBigEndian(static_cast<uint64_t>(input.size()) * 8, tail.data() + tailSize - lengthSize, lengthSize);

        for (std::size_t offset = 0; offset != tailSize; offset += blockSize)
            Compress(state, tail.data() + offset);

        Digest digest{};
        for (std::size_t word = 0; word != state.size(); ++word)
            WriteBigEndian(state[word], digest.data() + word * sizeof(uint32_t), sizeof(uint32_t));

        return digest;
    }

    void Sha256Software::Compress(State& state, const uint8_t* block)
    {
        std::array<uint32_t, 64> schedule{};

        for (std::size_t index = 0; index != 16; ++index)
            schedule[index] = ReadBigEndian(block + index * sizeof(uint32_t));

        for (std::size_t index = 16; index != schedule.size(); ++index)
        {
            const auto sigma0 = RotateRight(schedule[index - 15], 7) ^ RotateRight(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3);
            const auto sigma1 = RotateRight(schedule[index - 2], 17) ^ RotateRight(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10);
            schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
        }

        auto working = state;

        for (std::size_t index = 0; index != schedule.size(); ++index)
        {
            const auto sum1 = RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^ RotateRight(working[4], 25);
            const auto choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
            const auto first = working[7] + sum1 + choice + roundConstants[index] + schedule[index];
            const auto sum0 = RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^ RotateRight(working[0], 22);
            const auto majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
            const auto second = sum0 + majority;

            std::copy_backward(working.begin(), working.end() - 1, working.end());
            working[4] += first;
            working[0] = first + second;
        }

        for (std::size_t index = 0; index != state.size(); ++index)
            state[index] += working[index];
    }
}
