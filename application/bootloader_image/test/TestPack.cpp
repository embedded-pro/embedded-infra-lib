#include "application/bootloader_image/KeySet.hpp"
#include "application/bootloader_image/Pack.hpp"
#include "services/crypto/Sha256Software.hpp"
#include "uECC.h"
#include "upgrade/boot_loader/DecryptorAesTiny.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <cstring>
#include <gmock/gmock.h>
#include <string>
#include <variant>

namespace
{
    using namespace application;

    class TestPack
        : public ::testing::Test
    {
    public:
        TestPack()
        {
            auto generated = GenerateKeySet(random);
            EXPECT_TRUE(generated.has_value());
            keys = generated.value_or(KeySet{});
        }

        template<class T>
        static T ReadAt(const std::vector<uint8_t>& bytes, std::size_t offset)
        {
            T value{};
            EXPECT_LE(offset + sizeof(T), bytes.size());
            if (offset + sizeof(T) <= bytes.size())
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        static std::string Text(const std::array<char, 64>& field)
        {
            return std::string{ field.data(), strnlen(field.data(), field.size()) };
        }

        std::vector<uint8_t> Pack(const PackDescription& description)
        {
            const auto built = BuildPack(description, keys, random);
            EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(built));
            return std::holds_alternative<std::vector<uint8_t>>(built) ? std::get<std::vector<uint8_t>>(built) : std::vector<uint8_t>{};
        }

        std::vector<uint8_t> SignedContents(const std::vector<uint8_t>& pack) const
        {
            const auto prologue = ReadAt<UpgradePackHeaderPrologue>(pack, 0);
            const auto begin = sizeof(prologue) + prologue.signatureLength;
            if (begin + prologue.signedContentsLength > pack.size())
                return {};
            return { pack.begin() + static_cast<std::ptrdiff_t>(begin), pack.begin() + static_cast<std::ptrdiff_t>(begin + prologue.signedContentsLength) };
        }

        std::vector<uint8_t> DecryptedImagePart(const std::vector<uint8_t>& pack) const
        {
            const auto contents = SignedContents(pack);
            const auto counterStart = sizeof(UpgradePackHeaderEpilogue) + sizeof(ImageHeaderPrologue);
            const auto imageStart = counterStart + counterSize;
            if (contents.size() < imageStart)
                return {};
            const std::vector<uint8_t> counter{ contents.begin() + static_cast<std::ptrdiff_t>(counterStart), contents.begin() + static_cast<std::ptrdiff_t>(imageStart) };
            std::vector<uint8_t> encrypted{ contents.begin() + static_cast<std::ptrdiff_t>(imageStart), contents.end() };

            DecryptorAesTiny decryptor{ infra::MakeRange(keys.aesKey) };
            auto state = decryptor.StateBuffer();
            std::copy(counter.begin(), counter.end(), state.begin());
            decryptor.Reset();
            decryptor.DecryptPart(infra::MakeRange(encrypted));
            return encrypted;
        }

        uint32_t draws{ 0 };
        RandomSource random{ [this](std::span<uint8_t> bytes)
            {
                for (auto& byte : bytes)
                    byte = static_cast<uint8_t>(++draws * 41);
                return true;
            } };
        KeySet keys{};
        static constexpr std::size_t counterSize = 16;
        const std::vector<uint8_t> image{ 1, 2, 3, 4, 5, 6, 7 };
        const PackDescription description{ "test board", "1.7.0", "application", 0x00008000, image };
    };
}

TEST_F(TestPack, the_header_names_the_product_its_version_the_component_and_one_image)
{
    const auto pack = Pack(description);

    const auto prologue = ReadAt<UpgradePackHeaderPrologue>(pack, 0);
    EXPECT_EQ(upgradePackMagic, prologue.magic);
    EXPECT_EQ(UpgradePackStatus::readyToDeploy, prologue.status);
    EXPECT_EQ(2, prologue.signatureMethod);
    EXPECT_EQ(64, prologue.signatureLength);

    const auto epilogue = ReadAt<UpgradePackHeaderEpilogue>(pack, sizeof(prologue) + prologue.signatureLength);
    EXPECT_EQ(1, epilogue.headerVersion);
    EXPECT_EQ(sizeof(UpgradePackHeaderPrologue) + 64 + sizeof(UpgradePackHeaderEpilogue), epilogue.headerLength);
    EXPECT_EQ(1u, epilogue.numberOfImages);
    EXPECT_EQ("test board", Text(epilogue.productName));
    EXPECT_EQ("1.7.0", Text(epilogue.productVersion));
    EXPECT_EQ("application", Text(epilogue.componentName));
}

TEST_F(TestPack, the_signed_contents_verify_under_the_key_sets_public_key)
{
    const auto pack = Pack(description);
    const auto contents = SignedContents(pack);
    const auto digest = services::Sha256Software{}.Calculate(infra::MakeRange(contents));
    const auto signature = std::vector<uint8_t>{ pack.begin() + sizeof(UpgradePackHeaderPrologue), pack.begin() + sizeof(UpgradePackHeaderPrologue) + 64 };

    EXPECT_EQ(1, uECC_verify(keys.publicKey.data(), digest.data(), static_cast<unsigned>(digest.size()), signature.data(), uECC_secp256r1()));
}

TEST_F(TestPack, the_image_is_one_app_image_that_decrypts_to_its_destination_size_and_bytes)
{
    const auto pack = Pack(description);
    const auto contents = SignedContents(pack);
    const auto imagePrologue = ReadAt<ImageHeaderPrologue>(contents, sizeof(UpgradePackHeaderEpilogue));
    EXPECT_EQ("app", std::string(imagePrologue.targetName.data()));
    EXPECT_EQ(1u, imagePrologue.encryptionAndMacMethod);
    EXPECT_EQ(contents.size() - sizeof(UpgradePackHeaderEpilogue), imagePrologue.lengthOfHeaderAndImage);

    const auto decrypted = DecryptedImagePart(pack);
    const auto imageEpilogue = ReadAt<ImageHeaderEpilogue>(decrypted, 0);
    EXPECT_EQ(0x00008000u, imageEpilogue.destinationAddress);
    EXPECT_EQ(image.size(), imageEpilogue.imageSize);
    EXPECT_EQ(image, std::vector<uint8_t>(decrypted.begin() + sizeof(ImageHeaderEpilogue), decrypted.end()));
}

TEST_F(TestPack, a_name_that_does_not_fit_its_field_is_refused)
{
    auto tooLong = description;
    tooLong.productName = std::string(64, 'x');

    EXPECT_EQ(PackError::nameTooLong, std::get<PackError>(BuildPack(tooLong, keys, random)));
}

TEST_F(TestPack, a_failing_random_source_builds_no_pack)
{
    const RandomSource failing{ [](std::span<uint8_t>)
        {
            return false;
        } };

    EXPECT_EQ(PackError::randomSourceFailed, std::get<PackError>(BuildPack(description, keys, failing)));
}
