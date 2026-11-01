#include "upgrade/boot_loader/VerifierHashOnly.hpp"
#include "services/crypto/Sha256Software.hpp"

namespace application
{
    bool VerifierHashOnly::IsValid(hal::SynchronousFlash& flash, const hal::SynchronousFlash::Range& signature, const hal::SynchronousFlash::Range& data) const
    {
        std::array<uint8_t, 32> messageHash = Hash(flash, data);
        std::array<uint8_t, 32> storedHash;

        if (signature.second - signature.first != storedHash.size())
            return false;

        flash.ReadBuffer(storedHash, signature.first);

        return messageHash == storedHash;
    }

    std::array<uint8_t, 32> VerifierHashOnly::Hash(hal::SynchronousFlash& flash, const hal::SynchronousFlash::Range& data) const
    {
        services::Sha256SoftwareIncremental sha256;

        auto message = data.first;
        while (message != data.second)
        {
            std::array<uint8_t, 256> buffer;
            auto size = std::min<std::size_t>(buffer.size(), data.second - message);
            auto chunk = infra::Head(infra::MakeRange(buffer), size);
            flash.ReadBuffer(chunk, message);
            sha256.Update(chunk);
            message += size;
        }

        return sha256.Finish();
    }
}
