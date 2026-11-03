#include "application/bootloader_image/Pack.hpp"
#include "hal/synchronous_interfaces/SynchronousRandomDataGenerator.hpp"
#include "upgrade/pack_builder/ImageEncryptorAes.hpp"
#include "upgrade/pack_builder/ImageSignerEcDsa.hpp"
#include "upgrade/pack_builder/InputBinary.hpp"
#include "upgrade/pack_builder/UpgradePackBuilder.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

namespace application
{
    namespace
    {
        constexpr std::size_t nameFieldSize = 64;
        constexpr std::size_t signatureSize = 64;

        class RandomSourceGenerator
            : public hal::SynchronousRandomDataGenerator
        {
        public:
            explicit RandomSourceGenerator(const RandomSource& random)
                : random(random)
            {}

            void GenerateRandomData(infra::ByteRange result) override
            {
                if (!random(std::span<uint8_t>{ result.begin(), result.size() }))
                    failed = true;
            }

            bool Failed() const
            {
                return failed;
            }

        private:
            const RandomSource& random;
            bool failed{ false };
        };

        bool NamesFit(const PackDescription& pack)
        {
            return pack.productName.size() < nameFieldSize && pack.productVersion.size() < nameFieldSize && pack.componentName.size() < nameFieldSize;
        }

        std::array<uint8_t, signatureSize> SignatureSizedPrivateKey(const KeySet& keys)
        {
            std::array<uint8_t, signatureSize> privateKey{};
            std::ranges::copy(keys.privateKey, privateKey.begin());
            return privateKey;
        }

        std::vector<uint8_t> Build(const PackDescription& pack, const KeySet& keys, hal::SynchronousRandomDataGenerator& generator)
        {
            const auto privateKey = SignatureSizedPrivateKey(keys);
            ImageEncryptorAes encryptor{ generator, infra::MakeRange(keys.aesKey) };
            ImageSignerEcDsa256 signer{ generator, infra::MakeRange(keys.publicKey), infra::MakeRange(privateKey) };

            std::vector<std::unique_ptr<Input>> inputs;
            inputs.push_back(std::make_unique<InputBinary>("app", pack.image, pack.destination, encryptor));
            UpgradePackBuilder builder{ { pack.productName, pack.productVersion, pack.componentName, 0 }, std::move(inputs), signer };
            return builder.UpgradePack();
        }
    }

    std::variant<std::vector<uint8_t>, PackError> BuildPack(const PackDescription& pack, const KeySet& keys, const RandomSource& random)
    {
        if (!NamesFit(pack))
            return PackError::nameTooLong;

        RandomSourceGenerator generator{ random };
        try
        {
            auto bytes = Build(pack, keys, generator);
            if (generator.Failed())
                return PackError::randomSourceFailed;
            return bytes;
        }
        catch (const std::runtime_error&)
        {
            return generator.Failed() ? PackError::randomSourceFailed : PackError::signingFailed;
        }
    }
}
