#ifndef SERVICES_GATT_CLIENT_MOCK_HPP
#define SERVICES_GATT_CLIENT_MOCK_HPP

#include "services/ble/GattClient.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GattClientMock
        : public GattClient
    {
    public:
        MOCK_METHOD(std::size_t, MaxNumberOfConnections, (), (const override));
        MOCK_METHOD(std::size_t, NumberOfConnections, (), (const override));
    };

    class GattClientObserverMock
        : public GattClientObserver
    {
    public:
        using GattClientObserver::GattClientObserver;

        MOCK_METHOD(void, ConnectionEstablished, (infra::SharedPtr<GattClientConnection> connection), (override));
    };
}

#endif
