#ifndef BOOTLOADER_IMAGE_FORMAT_HPP
#define BOOTLOADER_IMAGE_FORMAT_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace bootloader
{
    inline constexpr uint32_t applicationHeaderOffset = 0x400;
    inline constexpr uint32_t imageDescriptorSize = 64;
    inline constexpr uint32_t imageStartCheckSize = 2048;
    inline constexpr std::size_t applicationTargetSize = 48;

    struct TargetLayout
    {
        uint32_t ramBegin;
        uint32_t ramEnd;
        uint32_t slotBegin;
        uint32_t slotEnd;
    };

    struct ApplicationHeader
    {
        static constexpr uint32_t validMagic = 0x48414d45;
        static constexpr uint16_t currentVersion = 1;

        uint32_t magic;
        uint16_t headerVersion;
        uint16_t reserved0;
        uint16_t versionMajor;
        uint16_t versionMinor;
        uint16_t versionPatch;
        uint16_t reserved1;
        std::array<uint8_t, applicationTargetSize> target;
    };

    static_assert(sizeof(ApplicationHeader) == 64);

    template<class Target>
    constexpr ApplicationHeader MakeApplicationHeader(const Target& target, uint16_t major, uint16_t minor, uint16_t patch)
    {
        static_assert(std::is_trivially_copyable_v<Target> && sizeof(Target) <= applicationTargetSize);

        ApplicationHeader header{ ApplicationHeader::validMagic, ApplicationHeader::currentVersion, 0xffff, major, minor, patch, 0xffff, {} };
        header.target.fill(0xff);
        const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(Target)>>(target);
        std::copy(bytes.begin(), bytes.end(), header.target.begin());
        return header;
    }

    template<class Target>
    Target TargetOf(const ApplicationHeader& header)
    {
        static_assert(std::is_trivially_copyable_v<Target> && sizeof(Target) <= applicationTargetSize);

        Target target{};
        std::memcpy(&target, header.target.data(), sizeof(target));
        return target;
    }

    class TargetCheck
    {
    protected:
        TargetCheck() = default;
        TargetCheck(const TargetCheck& other) = delete;
        TargetCheck& operator=(const TargetCheck& other) = delete;
        ~TargetCheck() = default;

    public:
        virtual bool Accepts(const ApplicationHeader& header) const = 0;
    };

    template<class Target>
    class ExactTargetCheck
        : public TargetCheck
    {
    public:
        explicit ExactTargetCheck(const Target& target)
            : target(target)
        {}

        bool Accepts(const ApplicationHeader& header) const override
        {
            return TargetOf<Target>(header) == target;
        }

    private:
        Target target;
    };

    struct ImageDescriptor
    {
        static constexpr uint32_t validMagic = 0x44494d45;

        uint32_t magic;
        uint32_t imageSize;
        std::array<uint8_t, 32> sha256;
        std::array<uint8_t, 24> reserved;
    };

    static_assert(sizeof(ImageDescriptor) == imageDescriptorSize);
}

#endif
