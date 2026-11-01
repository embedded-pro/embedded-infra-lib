#ifndef SERVICES_SHA256_SOFTWARE_HPP
#define SERVICES_SHA256_SOFTWARE_HPP

#include "services/crypto/Sha256.hpp"
#include <array>
#include <cstdint>

namespace services
{
    class Sha256Software
        : public Sha256
    {
    public:
        Digest Calculate(infra::ConstByteRange input) const override;
    };

    class Sha256SoftwareIncremental
    {
    public:
        Sha256SoftwareIncremental();

        void Update(infra::ConstByteRange input);
        Sha256::Digest Finish();

    private:
        static constexpr std::size_t blockSize = 64;

        using State = std::array<uint32_t, 8>;

        void Restart();
        void Pad();
        static void Compress(State& state, const uint8_t* block);

        State state{};
        std::array<uint8_t, blockSize> pending{};
        std::size_t pendingSize{};
        uint64_t messageSize{};
    };
}

#endif
