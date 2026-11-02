#include "application/bootloader_image/Pack.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/crypto/Sha256Software.hpp"
#include "uECC.h"
#include "upgrade/boot_loader/DecryptorAesTiny.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <algorithm>
#include <cstring>
#include <optional>
#include <string_view>

namespace application
{
    namespace
    {
        constexpr uint16_t headerVersion = 1;
        constexpr uint32_t encryptionAesCtr = 1;
        constexpr uint16_t signatureEcDsa256 = 2;
        constexpr std::size_t signatureSize = 64;

        template<class T>
        void Append(std::vector<uint8_t>& bytes, const T& value)
        {
            const auto range = infra::MakeByteRange(value);
            bytes.insert(bytes.end(), range.begin(), range.end());
        }

        bool CopyName(std::array<char, 64>& field, const std::string& name)
        {
            if (name.size() >= field.size())
                return false;

            field.fill(0);
            std::ranges::copy(name, field.begin());
            return true;
        }

        std::optional<UpgradePackHeaderEpilogue> Epilogue(const PackDescription& pack)
        {
            UpgradePackHeaderEpilogue epilogue{};
            epilogue.headerVersion = headerVersion;
            epilogue.headerLength = sizeof(UpgradePackHeaderPrologue) + signatureSize + sizeof(UpgradePackHeaderEpilogue);
            epilogue.numberOfImages = 1;
            if (!CopyName(epilogue.productName, pack.productName) || !CopyName(epilogue.productVersion, pack.productVersion) || !CopyName(epilogue.componentName, pack.componentName))
                return std::nullopt;
            return epilogue;
        }

        std::vector<uint8_t> EncryptedImage(const PackDescription& pack, const KeySet& keys, const InitialCounter& counter)
        {
            std::vector<uint8_t> image;
            Append(image, ImageHeaderEpilogue{ pack.destination, static_cast<uint32_t>(pack.image.size()) });
            image.insert(image.end(), pack.image.begin(), pack.image.end());
            image.resize(sizeof(ImageHeaderEpilogue) + (pack.image.size() + 3) / 4 * 4, 0);

            DecryptorAesTiny encryptor{ infra::MakeRange(keys.aesKey) };
            auto state = encryptor.StateBuffer();
            std::ranges::copy(counter, state.begin());
            encryptor.Reset();
            encryptor.DecryptPart(infra::MakeRange(image));
            return image;
        }

        std::vector<uint8_t> SignedContents(const UpgradePackHeaderEpilogue& epilogue, const std::vector<uint8_t>& image, const InitialCounter& counter)
        {
            ImageHeaderPrologue imagePrologue{};
            imagePrologue.lengthOfHeaderAndImage = static_cast<uint32_t>(sizeof(imagePrologue) + counter.size() + image.size());
            std::ranges::copy(std::string_view{ "app" }, imagePrologue.targetName.begin());
            imagePrologue.encryptionAndMacMethod = encryptionAesCtr;

            std::vector<uint8_t> contents;
            Append(contents, epilogue);
            Append(contents, imagePrologue);
            contents.insert(contents.end(), counter.begin(), counter.end());
            contents.insert(contents.end(), image.begin(), image.end());
            return contents;
        }

        std::vector<uint8_t> Prologue(std::size_t contentsSize)
        {
            UpgradePackHeaderPrologue prologue{};
            prologue.status = UpgradePackStatus::empty;
            prologue.magic = upgradePackMagic;
            prologue.errorCode = 0xffffffff;
            prologue.signedContentsLength = static_cast<uint32_t>(contentsSize);
            prologue.signatureMethod = signatureEcDsa256;
            prologue.signatureLength = signatureSize;

            std::vector<uint8_t> bytes;
            Append(bytes, prologue);
            return bytes;
        }
    }

    std::variant<std::vector<uint8_t>, PackError> BuildPack(const PackDescription& pack, const KeySet& keys, const InitialCounter& counter)
    {
        const auto epilogue = Epilogue(pack);
        if (!epilogue)
            return PackError::nameTooLong;

        const auto contents = SignedContents(*epilogue, EncryptedImage(pack, keys, counter), counter);
        const auto digest = services::Sha256Software{}.Calculate(infra::MakeRange(contents));

        std::array<uint8_t, signatureSize> signature{};
        if (uECC_sign(keys.privateKey.data(), digest.data(), static_cast<unsigned>(digest.size()), signature.data(), uECC_secp256r1()) != 1)
            return PackError::signingFailed;

        auto bytes = Prologue(contents.size());
        bytes.insert(bytes.end(), signature.begin(), signature.end());
        bytes.insert(bytes.end(), contents.begin(), contents.end());
        return bytes;
    }
}
