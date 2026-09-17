#ifndef SERVICES_GATT_SERVER_HPP
#define SERVICES_GATT_SERVER_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Endian.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/IntrusiveForwardList.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GattTypes.hpp"
#include <array>

namespace services
{
    class GattServerCharacteristicUpdate;

    class GattServerCharacteristicObserver
        : public infra::Observer<GattServerCharacteristicObserver, GattServerCharacteristicUpdate>
    {
    public:
        using infra::Observer<GattServerCharacteristicObserver, GattServerCharacteristicUpdate>::Observer;

        virtual void DataReceived(infra::ConstByteRange data) = 0;
    };

    class GattServerCharacteristicUpdate
        : public infra::Subject<GattServerCharacteristicObserver>
    {
    public:
        virtual void Update(infra::ConstByteRange data, infra::Function<void()> onDone) = 0;
    };

    class GattServerCharacteristicOperations;

    class GattServerCharacteristicOperationsObserver
        : public infra::Observer<GattServerCharacteristicOperationsObserver, GattServerCharacteristicOperations>
    {
    public:
        using infra::Observer<GattServerCharacteristicOperationsObserver, GattServerCharacteristicOperations>::Observer;

        virtual AttAttribute::Handle ServiceHandle() const = 0;
        virtual AttAttribute::Handle CharacteristicHandle() const = 0;
    };

    class GattServerCharacteristicOperations
        : public infra::Subject<GattServerCharacteristicOperationsObserver>
    {
    public:
        // Update 'characteristic' with 'data' towards the BLE stack and, depending on the
        // configuration of that 'characteristic', send a notification or indication.
        // Returns accepted, busy when the stack cannot take the update right now, or
        // another status on unrecoverable failure.
        virtual GattRequestStatus Update(const GattServerCharacteristicOperationsObserver& characteristic, infra::ConstByteRange data) const = 0;
    };

    class GattServerDescriptor
        : public infra::IntrusiveForwardList<GattServerDescriptor>::NodeType
        , public GattDescriptor
    {
    public:
        // These bit values are defined by this library, not by the Bluetooth Core
        // Specification.
        enum class AccessFlags : uint8_t
        {
            readOnly = 0x01u,
            writeReqOnly = 0x02u,
            readWrite = 0x03u,
            writeWithoutResponse = 0x04u,
            writeAny = 0x0Eu
        };

        GattServerDescriptor() = default;
        explicit GattServerDescriptor(const AttAttribute::Uuid& type, infra::ConstByteRange data);
        GattServerDescriptor(const AttAttribute::Uuid& uuid, const AccessFlags& access, infra::ConstByteRange data);

        infra::ConstByteRange Data() const;
        AccessFlags Access() const;

    private:
        AccessFlags access = AccessFlags::readOnly;
        infra::ConstByteRange data;
    };

    class GattServerCharacteristic
        : public infra::IntrusiveForwardList<GattServerCharacteristic>::NodeType
        , public GattCharacteristic
        , public GattServerCharacteristicOperationsObserver
        , public GattServerCharacteristicUpdate
    {
    public:
        // These bit values are defined by this library, not by the Bluetooth Core
        // Specification. Vol 3, Part F, section 3.2.5 describes attribute permissions as
        // a concept but assigns them no encoding, so nothing here can be checked against it.
        enum class PermissionFlags : uint8_t
        {
            none = 0x00u,
            authenticatedRead = 0x01u,
            authorizedRead = 0x02u,
            encryptedRead = 0x04u,
            authenticatedWrite = 0x08u,
            authorizedWrite = 0x10u,
            encryptedWrite = 0x20u
        };

        GattServerCharacteristic() = default;
        GattServerCharacteristic(const AttAttribute::Uuid& type, const PropertyFlags& properties, const PermissionFlags& permissions, uint16_t valueLength);

        PermissionFlags Permissions() const;
        uint16_t ValueLength() const;
        uint16_t GetAttributeCount() const;

        void AddDescriptor(GattServerDescriptor& descriptor);
        infra::IntrusiveForwardList<GattServerDescriptor>& Descriptors();
        const infra::IntrusiveForwardList<GattServerDescriptor>& Descriptors() const;

    protected:
        PermissionFlags permissions;
        uint16_t valueLength;
        infra::IntrusiveForwardList<GattServerDescriptor> descriptors;
    };

    class GattServerService;

    // An Include declaration needs its own node type: GattServerService is already a node of the
    // server's own list of services, and one object cannot be a node of two lists of the same
    // type. This refers to the included service rather than owning it.
    class GattServerIncludedService
        : public infra::IntrusiveForwardList<GattServerIncludedService>::NodeType
    {
    public:
        explicit GattServerIncludedService(GattServerService& service);

        GattServerService& Service();
        const GattServerService& Service() const;

    private:
        GattServerService& service;
    };

    class GattServerService
        : public GattService
        , public infra::IntrusiveForwardList<GattServerService>::NodeType
    {
    public:
        explicit GattServerService(const AttAttribute::Uuid& type);

        void AddCharacteristic(GattServerCharacteristic& characteristic);
        void RemoveCharacteristic(GattServerCharacteristic& characteristic);
        infra::IntrusiveForwardList<GattServerCharacteristic>& Characteristics();
        const infra::IntrusiveForwardList<GattServerCharacteristic>& Characteristics() const;

        void AddIncludedService(GattServerIncludedService& includedService);
        infra::IntrusiveForwardList<GattServerIncludedService>& IncludedServices();
        const infra::IntrusiveForwardList<GattServerIncludedService>& IncludedServices() const;

        uint16_t GetAttributeCount() const;

    private:
        infra::IntrusiveForwardList<GattServerCharacteristic> characteristics;
        infra::IntrusiveForwardList<GattServerIncludedService> includedServices;
    };

    class GattServer
    {
    public:
        GattServer() = default;
        GattServer(GattServer& other) = delete;
        GattServer& operator=(const GattServer& other) = delete;

        virtual void AddService(GattServerService& service) = 0;
    };

    inline GattServerCharacteristic::PermissionFlags operator|(GattServerCharacteristic::PermissionFlags lhs, GattServerCharacteristic::PermissionFlags rhs)
    {
        return static_cast<GattServerCharacteristic::PermissionFlags>(infra::enum_cast(lhs) | infra::enum_cast(rhs));
    }

    inline GattServerCharacteristic::PermissionFlags operator&(GattServerCharacteristic::PermissionFlags lhs, GattServerCharacteristic::PermissionFlags rhs)
    {
        return static_cast<GattServerCharacteristic::PermissionFlags>(infra::enum_cast(lhs) & infra::enum_cast(rhs));
    }
}

#endif
