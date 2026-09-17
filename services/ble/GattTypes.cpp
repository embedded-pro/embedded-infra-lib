#include "services/ble/GattTypes.hpp"

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
            default:
                return GattResult::unknown;
        }
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

    bool GattDescriptor::operator==(const GattDescriptor& other) const
    {
        return type == other.type && handle == other.handle;
    }

    bool GattDescriptor::operator!=(const GattDescriptor& other) const
    {
        return !(*this == other);
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

    TextOutputStream& operator<<(TextOutputStream& stream, const services::GattCharacteristic::PropertyFlags& properties)
    {
        stream << "[";
        if ((properties & services::GattCharacteristic::PropertyFlags::broadcast) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|broadcast|";
        if ((properties & services::GattCharacteristic::PropertyFlags::read) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|read|";
        if ((properties & services::GattCharacteristic::PropertyFlags::writeWithoutResponse) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|writeWithoutResponse|";
        if ((properties & services::GattCharacteristic::PropertyFlags::write) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|write|";
        if ((properties & services::GattCharacteristic::PropertyFlags::notify) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|notify|";
        if ((properties & services::GattCharacteristic::PropertyFlags::indicate) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|indicate|";
        if ((properties & services::GattCharacteristic::PropertyFlags::signedWrite) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|signedWrite|";
        if ((properties & services::GattCharacteristic::PropertyFlags::extended) != services::GattCharacteristic::PropertyFlags::none)
            stream << "|extended|";
        stream << "]";

        return stream;
    }
}
