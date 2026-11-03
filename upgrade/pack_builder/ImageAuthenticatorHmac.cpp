#include "upgrade/pack_builder/ImageAuthenticatorHmac.hpp"
#include "services/crypto/Sha256Software.hpp"
#include <algorithm>
#include <array>

namespace application
{
    namespace
    {
        constexpr std::size_t blockSize = 64;
        constexpr uint8_t innerPad = 0x36;
        constexpr uint8_t outerPad = 0x5c;

        using Block = std::array<uint8_t, blockSize>;

        Block BlockSizedKey(infra::ConstByteRange key)
        {
            Block block{};
            if (key.size() > blockSize)
            {
                const auto digest = services::Sha256Software{}.Calculate(key);
                std::ranges::copy(digest, block.begin());
            }
            else
                std::ranges::copy(key, block.begin());

            return block;
        }

        Block Padded(Block key, uint8_t pad)
        {
            std::ranges::transform(key, key.begin(), [pad](uint8_t byte)
                {
                    return static_cast<uint8_t>(byte ^ pad);
                });
            return key;
        }

        services::Sha256::Digest Hash(const Block& pad, infra::ConstByteRange message)
        {
            services::Sha256SoftwareIncremental sha256;
            sha256.Update(infra::MakeRange(pad));
            sha256.Update(message);
            return sha256.Finish();
        }
    }

    ImageAuthenticatorHmac::ImageAuthenticatorHmac(infra::ConstByteRange key)
        : key(key)
    {}

    std::vector<uint8_t> ImageAuthenticatorHmac::GenerateMac(const std::vector<uint8_t>& data) const
    {
        const auto blockSizedKey = BlockSizedKey(key);
        const auto inner = Hash(Padded(blockSizedKey, innerPad), infra::MakeRange(data));
        const auto mac = Hash(Padded(blockSizedKey, outerPad), infra::MakeRange(inner));

        return { mac.begin(), mac.end() };
    }
}
