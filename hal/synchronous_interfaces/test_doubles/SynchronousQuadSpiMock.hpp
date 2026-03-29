#ifndef SYNCHRONOUS_QUAD_SPI_MOCK_HPP
#define SYNCHRONOUS_QUAD_SPI_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousQuadSpi.hpp"
#include "gmock/gmock.h"
#include <vector>

namespace hal
{
    class SynchronousQuadSpiMock
        : public hal::SynchronousQuadSpi
    {
    public:
        void SendData(const Header& header, infra::ConstByteRange data) override;
        void SendDataQuad(const Header& header, infra::ConstByteRange data) override;
        void ReceiveData(const Header& header, infra::ByteRange data) override;
        void ReceiveDataQuad(const Header& header, infra::ByteRange data) override;

        MOCK_METHOD(void, SendDataMock, (const Header& header, std::vector<uint8_t> data));
        MOCK_METHOD(void, SendDataQuadMock, (const Header& header, std::vector<uint8_t> data));
        MOCK_METHOD(std::vector<uint8_t>, ReceiveDataMock, (const Header& header));
        MOCK_METHOD(std::vector<uint8_t>, ReceiveDataQuadMock, (const Header& header));
    };
}

#endif
