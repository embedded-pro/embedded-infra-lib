#ifndef HAL_ETHERNET_MOCK_HPP
#define HAL_ETHERNET_MOCK_HPP

#include "hal/interfaces/Ethernet.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class EthernetSmiMock
        : public EthernetSmi
    {
    public:
        MOCK_METHOD(uint16_t, PhyAddress, (), (const, override));
    };

    class EthernetMacMock
        : public EthernetMac
    {
    public:
        MOCK_METHOD(void, SendBuffer, (infra::ConstByteRange data, bool last), (override));
        MOCK_METHOD(void, RetryAllocation, (), (override));
        MOCK_METHOD(void, AddMacAddressFilter, (MacAddress address), (override));
        MOCK_METHOD(void, RemoveMacAddressFilter, (MacAddress address), (override));
    };
}

#endif
