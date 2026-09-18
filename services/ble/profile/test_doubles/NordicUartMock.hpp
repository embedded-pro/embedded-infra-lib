#ifndef SERVICES_NORDIC_UART_MOCK_HPP
#define SERVICES_NORDIC_UART_MOCK_HPP

#include "services/ble/profile/NordicUart.hpp"
#include "gmock/gmock.h"

namespace services
{
    class NordicUartMock
        : public NordicUart
    {
    public:
        MOCK_METHOD(bool, IsOpen, (), (const, override));
        MOCK_METHOD(std::size_t, MaxSendSize, (), (const, override));
        MOCK_METHOD(void, SendData, (infra::ConstByteRange data, infra::Function<void()> actionOnCompletion), (override));
        MOCK_METHOD(void, ReceiveData, (infra::Function<void(infra::ConstByteRange data)> dataReceived), (override));
    };

    class NordicUartObserverMock
        : public NordicUartObserver
    {
    public:
        using NordicUartObserver::NordicUartObserver;

        MOCK_METHOD(void, Opened, (), (override));
        MOCK_METHOD(void, Closed, (), (override));
    };
}

#endif
