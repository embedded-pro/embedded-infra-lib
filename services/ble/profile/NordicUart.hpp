#ifndef SERVICES_NORDIC_UART_HPP
#define SERVICES_NORDIC_UART_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/Att.hpp"

namespace services
{
    namespace uuid
    {
        const inline AttAttribute::Uuid128 nordicUartService{ { 0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
        const inline AttAttribute::Uuid128 nordicUartRx{ { 0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
        const inline AttAttribute::Uuid128 nordicUartTx{ { 0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
    }

    constexpr uint16_t attValueHeaderSize = 3;

    class NordicUart;

    class NordicUartObserver
        : public infra::Observer<NordicUartObserver, NordicUart>
    {
    public:
        using infra::Observer<NordicUartObserver, NordicUart>::Observer;

        virtual void Opened() = 0;
        virtual void Closed() = 0;
    };

    class NordicUart
        : public hal::SerialCommunication
        , public infra::Subject<NordicUartObserver>
    {
    public:
        virtual bool IsOpen() const = 0;
        virtual std::size_t MaxSendSize() const = 0;
    };
}

#endif
