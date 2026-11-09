#ifndef HAL_SERIAL_COMMUNICATION_NULL_HPP
#define HAL_SERIAL_COMMUNICATION_NULL_HPP

#include "hal/interfaces/SerialCommunication.hpp"

namespace hal
{
    class SerialCommunicationNull
        : public SerialCommunication
    {
    public:
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;
    };
}

#endif
