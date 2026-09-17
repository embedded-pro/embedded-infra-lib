#include "generated/echo/Dtm.pb.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/stream/ByteOutputStream.hpp"
#include "infra/syntax/ProtoFormatter.hpp"
#include "infra/syntax/ProtoParser.hpp"
#include "infra/util/EnumCast.hpp"
#include "services/ble/test_doubles/DirectTestModeMock.hpp"
#include "gmock/gmock.h"

namespace
{
    template<class Message>
    Message RoundTrip(const Message& message)
    {
        infra::ByteOutputStream::WithStorage<128> stream;
        infra::ProtoFormatter formatter(stream);
        message.Serialize(formatter);

        infra::ByteInputStream inputStream(stream.Writer().Processed());
        infra::ProtoParser parser(inputStream);

        return Message(parser);
    }
}

TEST(DirectTestModeTest, a_receiver_test_reports_the_packets_it_received)
{
    testing::StrictMock<services::DirectTestModeMock> dtm;

    EXPECT_CALL(dtm, StartReceiverTest(39, services::DirectTestMode::Phy::le2M, testing::_))
        .WillOnce(testing::DoAll(testing::InvokeArgument<2>(services::DirectTestMode::Result::success), testing::Return(services::DirectTestMode::RequestStatus::accepted)));
    EXPECT_EQ(services::DirectTestMode::RequestStatus::accepted, dtm.StartReceiverTest(services::DirectTestMode::channelMax, services::DirectTestMode::Phy::le2M, [](services::DirectTestMode::Result) {}));

    EXPECT_CALL(dtm, EndTest(testing::_))
        .WillOnce(testing::DoAll(testing::InvokeArgument<0>(services::DirectTestMode::Result::success, 1500), testing::Return(services::DirectTestMode::RequestStatus::accepted)));

    uint16_t received = 0;
    EXPECT_EQ(services::DirectTestMode::RequestStatus::accepted, dtm.EndTest([&received](services::DirectTestMode::Result, uint16_t packets)
                                                                     {
                                                                         received = packets;
                                                                     }));
    EXPECT_EQ(1500, received);
}

TEST(DirectTestModeTest, a_transmitter_test_names_its_payload_and_phy)
{
    testing::StrictMock<services::DirectTestModeMock> dtm;

    EXPECT_CALL(dtm, StartTransmitterTest(0, 37, services::DirectTestMode::PacketPayload::pseudoRandom9, services::DirectTestMode::Phy::leCodedS8, testing::_))
        .WillOnce(testing::DoAll(testing::InvokeArgument<4>(services::DirectTestMode::Result::success), testing::Return(services::DirectTestMode::RequestStatus::accepted)));

    EXPECT_EQ(services::DirectTestMode::RequestStatus::accepted, dtm.StartTransmitterTest(0, 37, services::DirectTestMode::PacketPayload::pseudoRandom9, services::DirectTestMode::Phy::leCodedS8, [](services::DirectTestMode::Result) {}));
}

TEST(DirectTestModeTest, a_refused_request_does_not_report_a_result)
{
    testing::StrictMock<services::DirectTestModeMock> dtm;

    EXPECT_CALL(dtm, StartUnmodulatedCarrier(2, 0, testing::_)).WillOnce(testing::Return(services::DirectTestMode::RequestStatus::notSupported));

    EXPECT_EQ(services::DirectTestMode::RequestStatus::notSupported, dtm.StartUnmodulatedCarrier(2, 0, [](services::DirectTestMode::Result)
                                                                         {
                                                                             FAIL() << "a refused request reports through its status, not through onDone";
                                                                         }));
}

TEST(DirectTestModeTest, the_transmit_power_is_signed_dBm)
{
    testing::StrictMock<services::DirectTestModeMock> dtm;

    EXPECT_CALL(dtm, SetTransmitPowerLevel(-20, testing::_))
        .WillOnce(testing::DoAll(testing::InvokeArgument<1>(services::DirectTestMode::Result::success), testing::Return(services::DirectTestMode::RequestStatus::accepted)));

    EXPECT_EQ(services::DirectTestMode::RequestStatus::accepted, dtm.SetTransmitPowerLevel(-20, [](services::DirectTestMode::Result) {}));
}

TEST(DirectTestModeTest, the_phy_matches_the_proto)
{
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::Phy::le1M), infra::enum_cast(dtm::Phy::PhyEnum::le1M));
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::Phy::le2M), infra::enum_cast(dtm::Phy::PhyEnum::le2M));
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::Phy::leCodedS8), infra::enum_cast(dtm::Phy::PhyEnum::leCodedS8));
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::Phy::leCodedS2), infra::enum_cast(dtm::Phy::PhyEnum::leCodedS2));
}

TEST(DirectTestModeTest, the_packet_payload_matches_the_proto_and_the_specification)
{
    EXPECT_EQ(0x00u, infra::enum_cast(services::DirectTestMode::PacketPayload::pseudoRandom9));
    EXPECT_EQ(0x07u, infra::enum_cast(services::DirectTestMode::PacketPayload::alternating0101));

    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::PacketPayload::pseudoRandom9), infra::enum_cast(dtm::PacketPayload::PacketPayloadEnum::pseudoRandom9));
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::PacketPayload::allOnes), infra::enum_cast(dtm::PacketPayload::PacketPayloadEnum::allOnes));
    EXPECT_EQ(infra::enum_cast(services::DirectTestMode::PacketPayload::alternating0101), infra::enum_cast(dtm::PacketPayload::PacketPayloadEnum::alternating0101));
}

TEST(DirectTestModeTest, round_trip_transmitter_parameters)
{
    dtm::TxParams parameters{ 17, 37,
        dtm::PacketPayload{ dtm::PacketPayload::PacketPayloadEnum::pseudoRandom15 },
        dtm::Phy{ dtm::Phy::PhyEnum::leCodedS2 }, 1000 };

    auto parsed = RoundTrip(parameters);

    EXPECT_EQ(17u, parsed.channel);
    EXPECT_EQ(dtm::Phy::PhyEnum::leCodedS2, parsed.phy.phy);
    EXPECT_EQ(parameters, parsed);
}

TEST(DirectTestModeTest, the_transmit_power_level_moved_to_a_new_method_id)
{
    // Its payload changed from a vendor power index to dBm, so the old id retires.
    EXPECT_EQ(7u, dtm::DtmProxy::idSetTxPowerLevel);
    EXPECT_EQ(4u, dtm::DtmProxy::idStartReceiverTest);
    EXPECT_EQ(5u, dtm::DtmProxy::idStartTransmitterTest);
    EXPECT_EQ(6u, dtm::DtmProxy::idEndTest);
}
