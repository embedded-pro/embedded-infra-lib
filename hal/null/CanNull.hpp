#ifndef HAL_CAN_NULL_HPP
#define HAL_CAN_NULL_HPP

#include "hal/interfaces/Can.hpp"

namespace hal
{
    class CanNull
        : public Can
    {
    public:
        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
    };
}

#endif
