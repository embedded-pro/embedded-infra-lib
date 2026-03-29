#include "hal/synchronous_interfaces/test_doubles/SynchronousQuadSpiMock.hpp"

namespace hal
{
    void SynchronousQuadSpiMock::SendData(const Header& header, infra::ConstByteRange data)
    {
        SendDataMock(header, std::vector<uint8_t>(data.begin(), data.end()));
    }

    void SynchronousQuadSpiMock::SendDataQuad(const Header& header, infra::ConstByteRange data)
    {
        SendDataQuadMock(header, std::vector<uint8_t>(data.begin(), data.end()));
    }

    void SynchronousQuadSpiMock::ReceiveData(const Header& header, infra::ByteRange data)
    {
        auto result = ReceiveDataMock(header);
        EXPECT_EQ(result.size(), data.size());
        std::copy(result.begin(), result.end(), data.begin());
    }

    void SynchronousQuadSpiMock::ReceiveDataQuad(const Header& header, infra::ByteRange data)
    {
        auto result = ReceiveDataQuadMock(header);
        EXPECT_EQ(result.size(), data.size());
        std::copy(result.begin(), result.end(), data.begin());
    }
}
