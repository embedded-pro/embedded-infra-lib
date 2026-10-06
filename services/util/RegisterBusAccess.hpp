#ifndef SERVICES_REGISTER_BUS_ACCESS_HPP
#define SERVICES_REGISTER_BUS_ACCESS_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    template<class Address>
    class GenericRegisterBusAccess
    {
    public:
        GenericRegisterBusAccess() = default;
        GenericRegisterBusAccess(const GenericRegisterBusAccess& other) = delete;
        GenericRegisterBusAccess& operator=(const GenericRegisterBusAccess& other) = delete;

    protected:
        ~GenericRegisterBusAccess() = default;

    public:
        virtual void ReadRegister(Address address, infra::ByteRange data, const infra::Function<void()>& onDone) = 0;
        virtual void WriteRegister(Address address, infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
    };

    using RegisterBusAccess = GenericRegisterBusAccess<uint8_t>;
    using RegisterBusAccessHalfWord = GenericRegisterBusAccess<uint16_t>;
}

#endif
