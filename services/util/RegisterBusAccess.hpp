#ifndef SERVICES_REGISTER_BUS_ACCESS_HPP
#define SERVICES_REGISTER_BUS_ACCESS_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    class RegisterBusAccess
    {
    public:
        RegisterBusAccess() = default;
        RegisterBusAccess(const RegisterBusAccess& other) = delete;
        RegisterBusAccess& operator=(const RegisterBusAccess& other) = delete;

    protected:
        ~RegisterBusAccess() = default;

    public:
        virtual void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) = 0;
        virtual void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
    };
}

#endif
