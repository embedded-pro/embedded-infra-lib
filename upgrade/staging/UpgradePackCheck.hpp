#ifndef UPGRADE_STAGING_UPGRADE_PACK_CHECK_HPP
#define UPGRADE_STAGING_UPGRADE_PACK_CHECK_HPP

#include "infra/util/BoundedString.hpp"
#include "upgrade/pack/UpgradePackHeader.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace application
{
    struct UpgradePackVersion
    {
        uint8_t major{};
        uint8_t minor{};
        uint8_t patch{};

        bool operator==(const UpgradePackVersion& other) const = default;
    };

    struct StagedPackHeader
    {
        UpgradePackHeaderPrologue prologue;
        UpgradePackHeaderEpilogue epilogue;
        ImageHeaderPrologue image;
    };

    struct StagedPackLimits
    {
        infra::BoundedConstString productName;
        infra::BoundedConstString targetName;
        uint32_t targetCapacity;
        uint32_t packSize;
    };

    constexpr uint32_t minimumUpgradePackSize{ sizeof(UpgradePackHeaderPrologue) + sizeof(UpgradePackHeaderEpilogue) };

    uint32_t UpgradePackEpilogueOffset(const UpgradePackHeaderPrologue& prologue);
    std::optional<UpgradePackVersion> ParseUpgradePackVersion(const std::array<char, 64>& text);
    std::optional<UpgradePackVersion> CheckStagedPackHeader(const StagedPackHeader& header, const StagedPackLimits& limits);
}

#endif
