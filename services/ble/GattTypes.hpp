#ifndef SERVICES_GATT_TYPES_HPP
#define SERVICES_GATT_TYPES_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/EnumCast.hpp"
#include "services/ble/Att.hpp"

namespace services
{
    enum class GattRequestStatus : uint8_t
    {
        accepted = 0,
        invalidState,
        invalidParameter,
        busy,
        notSupported
    };

    enum class GattResult : uint8_t
    {
        success = 0,
        invalidHandle,
        notPermitted,
        insufficientAuthentication,
        insufficientAuthorization,
        insufficientEncryption,
        insufficientResources,
        invalidLength,
        unsupported,
        disconnected,
        timeout,
        unknown
    };

    GattResult GattResultFromAttErrorCode(uint8_t attErrorCode);

    struct GattDescriptor
    {
        struct ClientCharacteristicConfiguration
        {
            static constexpr uint8_t valueHandleOffset = 1;
            static constexpr uint16_t attributeType = 0x2902;

            enum class CharacteristicValue : uint16_t
            {
                disable = 0x0000,
                enableNotification = 0x0001,
                enableIndication = 0x0002,
            };
        };

        GattDescriptor(const AttAttribute::Uuid& type, AttAttribute::Handle handle);
        GattDescriptor() = default;

        const AttAttribute::Uuid& Type() const;

        AttAttribute::Handle Handle() const;
        AttAttribute::Handle& Handle();

        bool operator==(const GattDescriptor& other) const = default;

    private:
        AttAttribute::Uuid type;
        AttAttribute::Handle handle;
    };

    namespace uuid
    {
        constexpr inline AttAttribute::Uuid16 deviceInformationService{ 0x180A };
        constexpr inline AttAttribute::Uuid16 systemId{ 0x2A23 };
        constexpr inline AttAttribute::Uuid16 modelNumber{ 0x2A24 };
        constexpr inline AttAttribute::Uuid16 serialNumber{ 0x2A25 };
        constexpr inline AttAttribute::Uuid16 firmwareRevision{ 0x2A26 };
        constexpr inline AttAttribute::Uuid16 hardwareRevision{ 0x2A27 };
        constexpr inline AttAttribute::Uuid16 softwareRevision{ 0x2A28 };
        constexpr inline AttAttribute::Uuid16 manufacturerName{ 0x2A29 };
        constexpr inline AttAttribute::Uuid16 ieeeCertification{ 0x2A2A };
        constexpr inline AttAttribute::Uuid16 pnpId{ 0x2A50 };
    }

    class GattCharacteristic
    {
    public:
        // Values taken from Bluetooth Core Specification
        // Volume 3, Part G, section 3.3.1.1
        enum class PropertyFlags : uint8_t
        {
            none = 0x00u,
            broadcast = 0x01u,
            read = 0x02u,
            writeWithoutResponse = 0x04u,
            write = 0x08u,
            notify = 0x10u,
            indicate = 0x20u,
            signedWrite = 0x40u,
            extended = 0x80u
        };

    public:
        GattCharacteristic(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, PropertyFlags properties);
        GattCharacteristic() = default;

        const PropertyFlags& Properties() const;
        const AttAttribute::Uuid& Type() const;

        AttAttribute::Handle Handle() const;
        AttAttribute::Handle& Handle();
        AttAttribute::Handle ValueHandle() const;
        AttAttribute::Handle& ValueHandle();

    private:
        static void AppendFlag(infra::TextOutputStream& stream, PropertyFlags properties, PropertyFlags flag, const char* name);

    public:
        friend infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const PropertyFlags& properties)
        {
            stream << "[";
            AppendFlag(stream, properties, PropertyFlags::broadcast, "broadcast");
            AppendFlag(stream, properties, PropertyFlags::read, "read");
            AppendFlag(stream, properties, PropertyFlags::writeWithoutResponse, "writeWithoutResponse");
            AppendFlag(stream, properties, PropertyFlags::write, "write");
            AppendFlag(stream, properties, PropertyFlags::notify, "notify");
            AppendFlag(stream, properties, PropertyFlags::indicate, "indicate");
            AppendFlag(stream, properties, PropertyFlags::signedWrite, "signedWrite");
            AppendFlag(stream, properties, PropertyFlags::extended, "extended");
            stream << "]";

            return stream;
        }

    private:
        AttAttribute::Uuid type;
        AttAttribute::Handle handle;
        AttAttribute::Handle valueHandle;
        PropertyFlags properties;
    };

    class GattService
    {
    public:
        explicit GattService(const AttAttribute::Uuid& type);
        GattService(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle);

        AttAttribute::Uuid Type() const;
        AttAttribute::Handle Handle() const;
        AttAttribute::Handle& Handle();
        AttAttribute::Handle EndHandle() const;
        AttAttribute::Handle& EndHandle();
        uint8_t GetAttributeCount() const;

    private:
        AttAttribute::Uuid type;
        AttAttribute::Handle handle;
        AttAttribute::Handle endHandle;
    };

    inline GattCharacteristic::PropertyFlags operator|(GattCharacteristic::PropertyFlags lhs, GattCharacteristic::PropertyFlags rhs)
    {
        return static_cast<GattCharacteristic::PropertyFlags>(infra::enum_cast(lhs) | infra::enum_cast(rhs));
    }

    inline GattCharacteristic::PropertyFlags operator&(GattCharacteristic::PropertyFlags lhs, GattCharacteristic::PropertyFlags rhs)
    {
        return static_cast<GattCharacteristic::PropertyFlags>(infra::enum_cast(lhs) & infra::enum_cast(rhs));
    }
}

namespace infra
{
    TextOutputStream& operator<<(TextOutputStream& stream, const services::AttAttribute::Uuid& uuid);
}

#endif
