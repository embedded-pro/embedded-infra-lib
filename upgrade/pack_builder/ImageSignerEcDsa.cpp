#include "upgrade/pack_builder/ImageSignerEcDsa.hpp"
#include "services/crypto/Sha256Software.hpp"
#include "uECC.h"

namespace application
{
    hal::SynchronousRandomDataGenerator* ImageSignerEcDsa::randomDataGenerator = nullptr;

    ImageSignerEcDsa::ImageSignerEcDsa(hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::ConstByteRange publicKey, infra::ConstByteRange privateKey)
        : publicKey(publicKey)
        , privateKey(privateKey)
        , replacedRandomNumberGenerator(uECC_get_rng())
    {
        ImageSignerEcDsa::randomDataGenerator = &randomDataGenerator;
        uECC_set_rng(RandomNumberGenerator);
    }

    ImageSignerEcDsa::~ImageSignerEcDsa()
    {
        uECC_set_rng(replacedRandomNumberGenerator);
    }

    std::vector<uint8_t> ImageSignerEcDsa::ImageSignature(const std::vector<uint8_t>& image)
    {
        CalculateSha256(image);
        CalculateSignature();
        return signature;
    }

    bool ImageSignerEcDsa::CheckSignature(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& image)
    {
        if (publicKey.size() != signature.size())
            return false;

        CalculateSha256(image);
        return uECC_verify(publicKey.begin(), hash.data(), static_cast<uint32_t>(hash.size()), signature.data(), GetCurve()) == 1;
    }

    void ImageSignerEcDsa::CalculateSha256(const std::vector<uint8_t>& image)
    {
        hash = services::Sha256Software{}.Calculate(infra::MakeRange(image));
    }

    void ImageSignerEcDsa::CalculateSignature()
    {
        signature.resize(SignatureLength());
        if (privateKey.size() != signature.size())
            throw std::runtime_error("Key length wrong");

        std::vector<uint8_t> publicKeyForVerification(SignatureLength());
        int result = uECC_compute_public_key(privateKey.begin(), publicKeyForVerification.data(), GetCurve());
        if (result != 1)
            throw std::runtime_error("Failed to compute public key");

        if (!std::equal(publicKey.begin(), publicKey.end(), publicKeyForVerification.data()))
            throw std::runtime_error("Inconsistent private key pair");

        int ret = uECC_sign(privateKey.begin(), hash.data(), static_cast<uint32_t>(hash.size()), signature.data(), GetCurve());
        if (ret != 1)
            throw std::runtime_error("Failed to calculate signature");
    }

    int ImageSignerEcDsa::RandomNumberGenerator(uint8_t* dest, unsigned size)
    {
        std::vector<uint8_t> entropy(size, 0);
        randomDataGenerator->GenerateRandomData(entropy);
        std::copy(entropy.begin(), entropy.end(), dest);
        return 1;
    }

    ImageSignerEcDsa224::ImageSignerEcDsa224(hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::ConstByteRange publicKey, infra::ConstByteRange privateKey)
        : ImageSignerEcDsa(randomDataGenerator, publicKey, privateKey)
    {}

    uint16_t ImageSignerEcDsa224::SignatureMethod() const
    {
        return signatureMethod;
    }

    uint16_t ImageSignerEcDsa224::SignatureLength() const
    {
        return static_cast<uint16_t>(keyLength / 8 * 2);
    }

    uECC_Curve ImageSignerEcDsa224::GetCurve() const
    {
        return uECC_secp224r1();
    }

    ImageSignerEcDsa256::ImageSignerEcDsa256(hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::ConstByteRange publicKey, infra::ConstByteRange privateKey)
        : ImageSignerEcDsa(randomDataGenerator, publicKey, privateKey)
    {}

    uint16_t ImageSignerEcDsa256::SignatureMethod() const
    {
        return signatureMethod;
    }

    uint16_t ImageSignerEcDsa256::SignatureLength() const
    {
        return static_cast<uint16_t>(keyLength / 8 * 2);
    }

    uECC_Curve ImageSignerEcDsa256::GetCurve() const
    {
        return uECC_secp256r1();
    }
}
