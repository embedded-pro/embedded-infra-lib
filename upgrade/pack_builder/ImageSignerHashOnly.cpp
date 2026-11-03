#include "upgrade/pack_builder/ImageSignerHashOnly.hpp"
#include "services/crypto/Sha256Software.hpp"

namespace application
{
    uint16_t ImageSignerHashOnly::SignatureMethod() const
    {
        return signatureMethod;
    }

    uint16_t ImageSignerHashOnly::SignatureLength() const
    {
        return static_cast<uint16_t>(hash.size());
    }

    std::vector<uint8_t> ImageSignerHashOnly::ImageSignature(const std::vector<uint8_t>& image)
    {
        CalculateSha256(image);

        return { hash.begin(), hash.end() };
    }

    bool ImageSignerHashOnly::CheckSignature(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& image)
    {
        return signature == ImageSignature(image);
    }

    void ImageSignerHashOnly::CalculateSha256(const std::vector<uint8_t>& image)
    {
        hash = services::Sha256Software{}.Calculate(infra::MakeRange(image));
    }
}
