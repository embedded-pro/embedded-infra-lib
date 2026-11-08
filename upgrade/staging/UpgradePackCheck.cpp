#include "upgrade/staging/UpgradePackCheck.hpp"
#include <algorithm>
#include <limits>

namespace application
{
    namespace
    {
        constexpr uint16_t headerVersion{ 1 };
        constexpr uint32_t aesCounterSize{ 16 };
        constexpr uint32_t imageOverhead{ sizeof(ImageHeaderPrologue) + aesCounterSize + sizeof(ImageHeaderEpilogue) };

        template<std::size_t N>
        infra::BoundedConstString TextOf(const std::array<char, N>& field)
        {
            const auto end = std::ranges::find(field, '\0');
            return infra::BoundedConstString{ field.data(), static_cast<std::size_t>(end - field.begin()) };
        }

        std::optional<uint8_t> ParseNumber(infra::BoundedConstString text)
        {
            if (text.empty())
                return std::nullopt;

            uint32_t value{ 0 };
            for (char digit : text)
            {
                if (digit < '0' || digit > '9')
                    return std::nullopt;

                value = value * 10 + static_cast<uint32_t>(digit - '0');
                if (value > std::numeric_limits<uint8_t>::max())
                    return std::nullopt;
            }

            return static_cast<uint8_t>(value);
        }

        bool FitsTheSlot(const ImageHeaderPrologue& image, const StagedPackLimits& limits)
        {
            return image.lengthOfHeaderAndImage >= imageOverhead && image.lengthOfHeaderAndImage - imageOverhead <= limits.targetCapacity;
        }

        bool NamesTheTarget(const StagedPackHeader& header, const StagedPackLimits& limits)
        {
            const bool oneTargetImage = header.epilogue.numberOfImages == 1 && TextOf(header.image.targetName) == limits.targetName;
            const bool imageEndsThePack = header.epilogue.headerLength + header.image.lengthOfHeaderAndImage == limits.packSize;
            return oneTargetImage && imageEndsThePack && FitsTheSlot(header.image, limits);
        }
    }

    uint32_t UpgradePackEpilogueOffset(const UpgradePackHeaderPrologue& prologue)
    {
        return sizeof(prologue) + prologue.signatureLength;
    }

    std::optional<UpgradePackVersion> ParseUpgradePackVersion(const std::array<char, 64>& text)
    {
        auto remaining = TextOf(text);
        std::array<uint8_t, 3> numbers{};

        for (std::size_t index = 0; index != numbers.size(); ++index)
        {
            const auto separator = remaining.find('.');
            const bool last = index + 1 == numbers.size();
            if (last != (separator == infra::BoundedConstString::npos))
                return std::nullopt;

            const auto number = ParseNumber(remaining.substr(0, separator));
            if (!number.has_value())
                return std::nullopt;

            numbers[index] = *number;
            remaining = last ? infra::BoundedConstString{} : remaining.substr(separator + 1);
        }

        return UpgradePackVersion{ numbers[0], numbers[1], numbers[2] };
    }

    std::optional<UpgradePackVersion> CheckStagedPackHeader(const StagedPackHeader& header, const StagedPackLimits& limits)
    {
        const auto& epilogue = header.epilogue;
        if (header.prologue.magic != upgradePackMagic || epilogue.headerVersion != headerVersion)
            return std::nullopt;

        if (epilogue.headerLength != UpgradePackEpilogueOffset(header.prologue) + sizeof(epilogue) || TextOf(epilogue.productName) != limits.productName)
            return std::nullopt;

        if (!NamesTheTarget(header, limits))
            return std::nullopt;

        return ParseUpgradePackVersion(epilogue.productVersion);
    }
}
