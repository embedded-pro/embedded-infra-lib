#include "upgrade/pack_builder/ImageEncryptorAes.hpp"
#include <array>

extern "C"
{
#include "crypto/tiny-aes128/TinyAes.h"
}

namespace application
{
    namespace
    {
        using Block = std::array<uint8_t, ImageEncryptorAes::blockLength>;

        void IncreaseCounter(Block& counter)
        {
            for (std::size_t i = counter.size(); i != 0; --i)
                if (++counter[i - 1] != 0)
                    break;
        }
    }

    WrongKeyLengthException::WrongKeyLengthException()
        : std::runtime_error("AES-128 needs a 16-byte key")
    {}

    ImageEncryptorAes::ImageEncryptorAes(hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::ConstByteRange key)
        : randomDataGenerator(randomDataGenerator)
        , key(key)
    {}

    uint32_t ImageEncryptorAes::EncryptionAndMacMethod() const
    {
        return encryptionAndMacMethod;
    }

    std::vector<uint8_t> ImageEncryptorAes::Secure(const std::vector<uint8_t>& data) const
    {
        if (key.size() != keyLength)
            throw WrongKeyLengthException();

        Block counter{};
        randomDataGenerator.GenerateRandomData(infra::MakeRange(counter));

        std::vector<uint8_t> result{ counter.begin(), counter.end() };
        Block keyStream{};
        for (std::size_t i = 0; i != data.size(); ++i)
        {
            if (i % blockLength == 0)
            {
                AES128_ECB_encrypt(counter.data(), key.begin(), keyStream.data());
                IncreaseCounter(counter);
            }

            result.push_back(static_cast<uint8_t>(data[i] ^ keyStream[i % blockLength]));
        }

        return result;
    }
}
