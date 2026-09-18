#ifndef SERVICES_NORDIC_UART_HPP
#define SERVICES_NORDIC_UART_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/Att.hpp"

namespace services
{
    namespace uuid
    {
        // Nordic Semiconductor's UART Service, built on the vendor base
        // 6E400001-B5A3-F393-E0A9-E50E24DCCA9E. These values are assigned by Nordic and appear in
        // no Bluetooth SIG document; Assigned Numbers does not list them.
        //
        // Rx and Tx are named from the peripheral's point of view: a central writes into Rx and
        // subscribes to Tx.
        //
        // AttAttribute::Uuid128 cannot be constexpr, because infra::SwapEndian over a std::array
        // is not.
        const inline AttAttribute::Uuid128 nordicUartService{ { 0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
        const inline AttAttribute::Uuid128 nordicUartRx{ { 0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
        const inline AttAttribute::Uuid128 nordicUartTx{ { 0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93,
            0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E } };
    }

    // A Handle Value Notification and a Write Command each spend one octet on the opcode and two
    // on the handle before the value starts.
    // Bluetooth Core Specification, Volume 3, Part F, sections 3.4.7.1 and 3.4.5.3
    constexpr uint16_t attValueHeaderSize = 3;

    class NordicUart;

    class NordicUartObserver
        : public infra::Observer<NordicUartObserver, NordicUart>
    {
    public:
        using infra::Observer<NordicUartObserver, NordicUart>::Observer;

        // The pipe carries data from here until Closed.
        virtual void Opened() = 0;

        // Either the peer unsubscribed or the link is gone. A send that was in progress has
        // already reported completion.
        virtual void Closed() = 0;
    };

    // The Nordic UART Service as a serial port, so that everything in this repository which speaks
    // hal::SerialCommunication speaks it over BLE unchanged. Both GATT roles implement this.
    class NordicUart
        : public hal::SerialCommunication
        , public infra::Subject<NordicUartObserver>
    {
    public:
        virtual bool IsOpen() const = 0;

        // The largest value that fits one notification or write, ATT_MTU - 3. SendData splits
        // anything larger.
        virtual std::size_t MaxSendSize() const = 0;
    };
}

#endif
