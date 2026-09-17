#ifndef SERVICES_GATT_CLIENT_HPP
#define SERVICES_GATT_CLIENT_HPP

#include "infra/util/SharedPtr.hpp"
#include "services/ble/GattClientConnection.hpp"

namespace services
{
    class GattClient;

    class GattClientObserver
        : public infra::Observer<GattClientObserver, GattClient>
    {
    public:
        using infra::Observer<GattClientObserver, GattClient>::Observer;

        virtual void ConnectionEstablished(infra::SharedPtr<GattClientConnection> connection) = 0;

        // Reported when the link is gone. An observer that kept the SharedPtr it was given lets go
        // of it here, since the connection holds its slot until the last holder does.
        virtual void ConnectionReleased(GattClientConnection& connection) = 0;
    };

    class GattClient
        : public infra::Subject<GattClientObserver>
    {
    public:
        virtual std::size_t MaxNumberOfConnections() const = 0;
        virtual std::size_t NumberOfConnections() const = 0;
    };
}

#endif
