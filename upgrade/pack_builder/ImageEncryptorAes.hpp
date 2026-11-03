#ifndef UPGRADE_IMAGE_ENCRYPTOR_AES_HPP
#define UPGRADE_IMAGE_ENCRYPTOR_AES_HPP

#include "hal/synchronous_interfaces/SynchronousRandomDataGenerator.hpp"
#include "infra/util/ByteRange.hpp"
#include "upgrade/pack_builder/ImageSecurity.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace application
{
    class WrongKeyLengthException
        : public std::runtime_error
    {
    public:
        WrongKeyLengthException();
    };

    class ImageEncryptorAes
        : public ImageSecurity
    {
    public:
        static const uint32_t encryptionAndMacMethod = 1;
        static const std::size_t blockLength = 16;
        static const std::size_t keyLength = 16;

        ImageEncryptorAes(hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::ConstByteRange key);

        uint32_t EncryptionAndMacMethod() const override;
        std::vector<uint8_t> Secure(const std::vector<uint8_t>& data) const override;

    private:
        hal::SynchronousRandomDataGenerator& randomDataGenerator;
        infra::ConstByteRange key;
    };
}

#endif
