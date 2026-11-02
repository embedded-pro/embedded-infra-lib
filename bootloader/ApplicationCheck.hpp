#ifndef BOOTLOADER_APPLICATION_CHECK_HPP
#define BOOTLOADER_APPLICATION_CHECK_HPP

#include "bootloader/ImageFormat.hpp"
#include "infra/util/ByteRange.hpp"

namespace bootloader
{
    bool ImageStartFitsTarget(infra::ConstByteRange imageStart, const TargetLayout& layout, const TargetCheck& target);
    bool ApplicationIsStartable(infra::ConstByteRange slot, const TargetLayout& layout, const TargetCheck& target);
    ImageDescriptor MakeImageDescriptor(infra::ConstByteRange image);
}

#endif
