#include "services/ble/GattTypes.hpp"
#include "infra/stream/ByteInputStream.hpp"

namespace services
{
    GattResult GattResultFromAttErrorCode(uint8_t attErrorCode)
    {
        switch (static_cast<AttErrorCode>(attErrorCode))
        {
            case AttErrorCode::success:
                return GattResult::success;
            case AttErrorCode::invalidHandle:
            case AttErrorCode::attributeNotFound:
                return GattResult::invalidHandle;
            case AttErrorCode::readNotPermitted:
            case AttErrorCode::writeNotPermitted:
                return GattResult::notPermitted;
            case AttErrorCode::insufficientAuthentication:
                return GattResult::insufficientAuthentication;
            case AttErrorCode::insufficientAuthorization:
                return GattResult::insufficientAuthorization;
            case AttErrorCode::insufficientEncryptionKeySize:
            case AttErrorCode::insufficientEncryption:
                return GattResult::insufficientEncryption;
            case AttErrorCode::prepareQueueFull:
            case AttErrorCode::insufficientResources:
                return GattResult::insufficientResources;
            case AttErrorCode::invalidOffset:
            case AttErrorCode::attributeNotLong:
            case AttErrorCode::invalidAttributeValueLength:
                return GattResult::invalidLength;
            case AttErrorCode::requestNotSupported:
            case AttErrorCode::unsupportedGroupType:
                return GattResult::unsupported;
            case AttErrorCode::databaseOutOfSync:
                return GattResult::databaseOutOfSync;
            case AttErrorCode::valueNotAllowed:
                return GattResult::valueNotAllowed;
            // unlikelyError is the specification's own name for a failure with no more
            // specific cause, so unknown is the faithful translation rather than a lossy one.
            default:
                return GattResult::unknown;
        }
    }

    std::optional<GattServiceChanged> GattServiceChangedFromValue(infra::ConstByteRange value)
    {
        // Decoded byte-wise through the stream rather than by casting the payload, so that this
        // is correct on a big-endian host and cannot fault on an unaligned payload.
        infra::ByteInputStream stream(value, infra::softFail);

        GattServiceChanged serviceChanged{};
        serviceChanged.startHandle = infra::FromLittleEndian(stream.Extract<AttAttribute::Handle>());
        serviceChanged.endHandle = infra::FromLittleEndian(stream.Extract<AttAttribute::Handle>());

        if (stream.Failed() || !stream.Empty())
            return std::nullopt;

        return std::make_optional(serviceChanged);
    }

    GattDescriptor::GattDescriptor(const AttAttribute::Uuid& type, AttAttribute::Handle handle)
        : type(type)
        , handle(handle)
    {}

    const AttAttribute::Uuid& GattDescriptor::Type() const
    {
        return type;
    }

    AttAttribute::Handle GattDescriptor::Handle() const
    {
        return handle;
    }

    AttAttribute::Handle& GattDescriptor::Handle()
    {
        return handle;
    }

    GattCharacteristic::GattCharacteristic(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties)
        : type(type)
        , handle(handle)
        , valueHandle(valueHandle)
        , properties(properties)
    {}

    const GattCharacteristic::PropertyFlags& GattCharacteristic::Properties() const
    {
        return properties;
    }

    const AttAttribute::Uuid& GattCharacteristic::Type() const
    {
        return type;
    }

    AttAttribute::Handle GattCharacteristic::Handle() const
    {
        return handle;
    }

    AttAttribute::Handle& GattCharacteristic::Handle()
    {
        return handle;
    }

    AttAttribute::Handle GattCharacteristic::ValueHandle() const
    {
        return valueHandle;
    }

    AttAttribute::Handle& GattCharacteristic::ValueHandle()
    {
        return valueHandle;
    }

    GattService::GattService(const AttAttribute::Uuid& type)
        : GattService(type, 0, 0)
    {}

    GattService::GattService(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle)
        : type(type)
        , handle(handle)
        , endHandle(endHandle)
    {}

    AttAttribute::Uuid GattService::Type() const
    {
        return type;
    }

    AttAttribute::Handle GattService::Handle() const
    {
        return handle;
    }

    AttAttribute::Handle& GattService::Handle()
    {
        return handle;
    }

    AttAttribute::Handle GattService::EndHandle() const
    {
        return endHandle;
    }

    AttAttribute::Handle& GattService::EndHandle()
    {
        return endHandle;
    }

    uint8_t GattService::GetAttributeCount() const
    {
        return 0;
    }

    void GattCharacteristic::AppendFlag(infra::TextOutputStream& stream, PropertyFlags properties, PropertyFlags flag, const char* name)
    {
        if ((properties & flag) != PropertyFlags::none)
            stream << "|" << name << "|";
    }
}

namespace infra
{
    TextOutputStream& operator<<(TextOutputStream& stream, const services::AttAttribute::Uuid& uuid)
    {
        if (std::holds_alternative<services::AttAttribute::Uuid16>(uuid))
            stream << "[" << hex << std::get<services::AttAttribute::Uuid16>(uuid) << "]";
        else
            stream << "[" << AsHex(MakeByteRange(std::get<services::AttAttribute::Uuid128>(uuid))) << "]";

        return stream;
    }
}
