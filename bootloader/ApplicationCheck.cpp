#include "bootloader/ApplicationCheck.hpp"
#include "services/crypto/Sha256Software.hpp"
#include <cstring>

namespace bootloader
{
    namespace
    {
        constexpr uint32_t thumbBit = 1;

        template<class T>
        T Read(infra::ConstByteRange bytes, std::size_t offset)
        {
            T value{};
            std::memcpy(&value, bytes.begin() + offset, sizeof(value));
            return value;
        }

        bool VectorsFitTarget(infra::ConstByteRange imageStart, const TargetLayout& layout)
        {
            const auto stackPointer = Read<uint32_t>(imageStart, 0);
            const auto resetHandler = Read<uint32_t>(imageStart, 4);
            const auto resetAddress = resetHandler & ~thumbBit;

            return stackPointer > layout.ramBegin && stackPointer <= layout.ramEnd && (resetHandler & thumbBit) != 0 &&
                   resetAddress >= layout.slotBegin && resetAddress < layout.slotEnd - imageDescriptorSize;
        }

        bool HeaderNamesTarget(infra::ConstByteRange imageStart, const TargetCheck& target)
        {
            const auto header = Read<ApplicationHeader>(imageStart, applicationHeaderOffset);

            return header.magic == ApplicationHeader::validMagic && header.headerVersion == ApplicationHeader::currentVersion && target.Accepts(header);
        }
    }

    bool ImageStartFitsTarget(infra::ConstByteRange imageStart, const TargetLayout& layout, const TargetCheck& target)
    {
        return imageStart.size() >= applicationHeaderOffset + sizeof(ApplicationHeader) && VectorsFitTarget(imageStart, layout) &&
               HeaderNamesTarget(imageStart, target);
    }

    bool ApplicationIsStartable(infra::ConstByteRange slot, const TargetLayout& layout, const TargetCheck& target)
    {
        if (slot.size() < imageDescriptorSize)
            return false;

        const auto descriptor = Read<ImageDescriptor>(slot, slot.size() - imageDescriptorSize);
        if (descriptor.magic != ImageDescriptor::validMagic || descriptor.imageSize > slot.size() - imageDescriptorSize)
            return false;

        const auto image = infra::Head(slot, descriptor.imageSize);
        return services::Sha256Software{}.Calculate(image) == descriptor.sha256 && ImageStartFitsTarget(image, layout, target);
    }

    ImageDescriptor MakeImageDescriptor(infra::ConstByteRange image)
    {
        ImageDescriptor descriptor{ ImageDescriptor::validMagic, static_cast<uint32_t>(image.size()), services::Sha256Software{}.Calculate(image), {} };
        descriptor.reserved.fill(0xff);
        return descriptor;
    }
}
