#ifndef APPLICATION_BOOTLOADER_IMAGE_COMMAND_HPP
#define APPLICATION_BOOTLOADER_IMAGE_COMMAND_HPP

#include "application/bootloader_image/KeySet.hpp"
#include <ostream>
#include <string>
#include <vector>

namespace application
{
    int Run(const std::vector<std::string>& arguments, std::ostream& out, std::ostream& err, const RandomSource& random);
}

#endif
