#include "application/bootloader_image/Image.hpp"
#include "bootloader/ApplicationCheck.hpp"
#include <cstring>
#include <optional>

namespace application
{
    namespace
    {
        const std::string endOfFile{ ":00000001FF" };
        const std::vector<std::string> addressesFromZero{ ":020000020000FC", ":020000040000FA" };

        std::string Trimmed(const std::string& line)
        {
            const auto first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            return line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
        }

        bool IsRecord(const std::string& line)
        {
            return line.size() >= endOfFile.size() && line[0] == ':';
        }

        bool IsStartAddress(const std::string& type)
        {
            return type == "03" || type == "05";
        }

        std::optional<std::size_t> Append(std::vector<std::string>& combined, const std::vector<std::string>& lines, bool keepStartAddress)
        {
            for (std::size_t index = 0; index < lines.size(); ++index)
            {
                const auto line = Trimmed(lines[index]);
                if (line.empty())
                    continue;
                if (!IsRecord(line))
                    return index + 1;

                const auto type = line.substr(7, 2);
                if (type == "01" || (IsStartAddress(type) && !keepStartAddress))
                    continue;
                combined.push_back(line);
            }
            return std::nullopt;
        }
    }

    std::vector<uint8_t> ImageDescriptorFor(std::span<const uint8_t> image)
    {
        const auto descriptor = bootloader::MakeImageDescriptor(infra::ConstByteRange{ image.data(), image.data() + image.size() });
        std::vector<uint8_t> bytes(sizeof(descriptor));
        std::memcpy(bytes.data(), &descriptor, sizeof(descriptor));
        return bytes;
    }

    std::variant<std::vector<std::string>, CombineError> Combine(const std::vector<std::string>& bootloader, const std::vector<std::string>& application)
    {
        std::vector<std::string> combined;

        if (const auto line = Append(combined, bootloader, true))
            return CombineError{ CombineError::Source::bootloader, *line };

        combined.insert(combined.end(), addressesFromZero.begin(), addressesFromZero.end());

        if (const auto line = Append(combined, application, false))
            return CombineError{ CombineError::Source::application, *line };

        combined.push_back(endOfFile);
        return combined;
    }
}
