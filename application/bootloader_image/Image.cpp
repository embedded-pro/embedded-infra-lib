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

        constexpr uint8_t data = 0x00;
        constexpr uint8_t extendedSegmentAddress = 0x02;
        constexpr uint8_t extendedLinearAddress = 0x04;

        struct Record
        {
            uint16_t address;
            uint8_t type;
            std::vector<uint8_t> data;
        };

        std::optional<uint8_t> ByteAt(const std::string& line, std::size_t index)
        {
            const auto digit = [](char c) -> int
            {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                return -1;
            };

            const auto high = digit(line[1 + 2 * index]);
            const auto low = digit(line[2 + 2 * index]);
            if (high < 0 || low < 0)
                return std::nullopt;
            return static_cast<uint8_t>(high * 16 + low);
        }

        std::optional<std::vector<uint8_t>> RecordBytes(const std::string& line)
        {
            if (!IsRecord(line) || line.size() % 2 == 0)
                return std::nullopt;

            std::vector<uint8_t> bytes;
            for (std::size_t index = 0; index < (line.size() - 1) / 2; ++index)
            {
                const auto byte = ByteAt(line, index);
                if (!byte)
                    return std::nullopt;
                bytes.push_back(*byte);
            }
            return bytes;
        }

        std::optional<Record> ParseRecord(const std::string& line)
        {
            const auto bytes = RecordBytes(line);
            if (!bytes || bytes->size() != std::size_t{ bytes->front() } + 5)
                return std::nullopt;

            uint8_t sum = 0;
            for (auto byte : *bytes)
                sum = static_cast<uint8_t>(sum + byte);
            if (sum != 0)
                return std::nullopt;

            return Record{ static_cast<uint16_t>((*bytes)[1] << 8 | (*bytes)[2]), (*bytes)[3], { bytes->begin() + 4, bytes->end() - 1 } };
        }

        std::string Format(const Record& record)
        {
            static constexpr char digits[] = "0123456789ABCDEF";
            std::vector<uint8_t> bytes{ static_cast<uint8_t>(record.data.size()), static_cast<uint8_t>(record.address >> 8), static_cast<uint8_t>(record.address), record.type };
            bytes.insert(bytes.end(), record.data.begin(), record.data.end());

            uint8_t sum = 0;
            for (auto byte : bytes)
                sum = static_cast<uint8_t>(sum + byte);
            bytes.push_back(static_cast<uint8_t>(0x100 - sum));

            std::string line{ ":" };
            for (auto byte : bytes)
                line += { digits[byte >> 4], digits[byte & 0x0f] };
            return line;
        }

        uint32_t BaseAddress(const Record& record)
        {
            if (record.data.size() != 2)
                return 0;
            const auto value = static_cast<uint32_t>(record.data[0] << 8 | record.data[1]);
            return record.type == extendedLinearAddress ? value << 16 : value << 4;
        }

        bool Overwrite(Record& record, uint32_t base, uint32_t address, std::span<const uint8_t> bytes, std::size_t& replaced)
        {
            bool changed = false;
            for (std::size_t index = 0; index < record.data.size(); ++index)
            {
                const uint64_t at = uint64_t{ base } + record.address + index;
                if (at >= address && at < uint64_t{ address } + bytes.size())
                {
                    record.data[index] = bytes[at - address];
                    ++replaced;
                    changed = true;
                }
            }
            return changed;
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

    bool PatchHex(std::vector<std::string>& lines, uint32_t address, std::span<const uint8_t> bytes)
    {
        auto patched = lines;
        uint32_t base = 0;
        std::size_t replaced = 0;

        for (auto& line : patched)
        {
            const auto trimmed = Trimmed(line);
            if (trimmed.empty())
                continue;

            auto record = ParseRecord(trimmed);
            if (!record)
                return false;

            if (record->type == extendedSegmentAddress || record->type == extendedLinearAddress)
                base = BaseAddress(*record);
            else if (record->type == data && Overwrite(*record, base, address, bytes, replaced))
                line = Format(*record) + (line.ends_with('\r') ? "\r" : "");
        }

        if (replaced != bytes.size())
            return false;

        lines = std::move(patched);
        return true;
    }
}
