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

    private:
        using State = std::array<uint32_t, 8>;

        static void Compress(State& state, const uint8_t* block);
    };
}

#endif
