#include "application/bootloader_image/Command.hpp"
#include <iostream>
#include <random>

int main(int argc, char* argv[])
{
    std::random_device device;
    const application::RandomSource random{ [&device](std::span<uint8_t> bytes)
        {
            for (auto& byte : bytes)
                byte = static_cast<uint8_t>(device());
            return true;
        } };

    return application::Run(std::vector<std::string>(argv + 1, argv + argc), std::cout, std::cerr, random);
}
