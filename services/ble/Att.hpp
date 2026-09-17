#ifndef SERVICES_ATT_HPP
#define SERVICES_ATT_HPP

#include "infra/util/Endian.hpp"
#include <array>
#include <cstdint>
#include <variant>

namespace services
{
    struct AttAttribute
    {
        using Uuid16 = uint16_t;
        using Uuid128 = infra::BigEndian<std::array<uint8_t, 16>>;
        using Uuid = std::variant<Uuid16, Uuid128>;

        using Handle = uint16_t;
    };

    constexpr uint16_t attDefaultMaxMtuSize = 23;

    // Values taken from Bluetooth Core Specification
    // Volume 3, Part F, section 3.4.1.1
    enum class AttErrorCode : uint8_t
    {
        success = 0x00u,
        invalidHandle = 0x01u,
        readNotPermitted = 0x02u,
        writeNotPermitted = 0x03u,
        invalidPdu = 0x04u,
        insufficientAuthentication = 0x05u,
        requestNotSupported = 0x06u,
        invalidOffset = 0x07u,
        insufficientAuthorization = 0x08u,
        prepareQueueFull = 0x09u,
        attributeNotFound = 0x0au,
        attributeNotLong = 0x0bu,
        insufficientEncryptionKeySize = 0x0cu,
        invalidAttributeValueLength = 0x0du,
        unlikelyError = 0x0eu,
        insufficientEncryption = 0x0fu,
        unsupportedGroupType = 0x10u,
        insufficientResources = 0x11u,
        databaseOutOfSync = 0x12u,
        valueNotAllowed = 0x13u
    };
}

#endif
