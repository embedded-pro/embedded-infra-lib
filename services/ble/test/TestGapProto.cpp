#include "generated/echo/GapCentral.pb.hpp"
#include "generated/echo/GapPeripheral.pb.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/stream/ByteOutputStream.hpp"
#include "infra/syntax/ProtoFormatter.hpp"
#include "infra/syntax/ProtoParser.hpp"
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

TEST(GapProtoTest, round_trip_accepted_central_completion)
{
    gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::accepted }, gap::central::Completion::Result::timeout };

    EXPECT_EQ(completion, RoundTrip(completion));
}

TEST(GapProtoTest, round_trip_rejected_central_completion)
{
    gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::invalidState }, gap::central::Completion::Result::success };

    auto parsed = RoundTrip(completion);

    EXPECT_EQ(gap::central::RequestStatus::Status::invalidState, parsed.requestStatus.status);
    EXPECT_EQ(gap::central::Completion::Result::success, parsed.result);
}

TEST(GapProtoTest, central_completion_carries_every_gap_central_result)
{
    for (auto result : { gap::central::Completion::Result::success, gap::central::Completion::Result::cancelled,
             gap::central::Completion::Result::timeout, gap::central::Completion::Result::connectionFailed,
             gap::central::Completion::Result::controllerError })
    {
        gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::accepted }, result };

        EXPECT_EQ(result, RoundTrip(completion).result);
    }
}

TEST(GapProtoTest, peripheral_completion_carries_every_gap_peripheral_result)
{
    for (auto result : { gap::peripheral::Completion::Result::success, gap::peripheral::Completion::Result::invalidParameter,
             gap::peripheral::Completion::Result::controllerError })
    {
        gap::peripheral::Completion completion{ gap::peripheral::RequestStatus{ gap::peripheral::RequestStatus::Status::accepted }, result };

        EXPECT_EQ(result, RoundTrip(completion).result);
    }
}

TEST(GapProtoTest, round_trip_pairing_completion)
{
    gap::peripheral::PairingCompletion completion{ gap::peripheral::RequestStatus{ gap::peripheral::RequestStatus::Status::accepted },
        gap::peripheral::PairingStatus{ gap::peripheral::PairingStatus::Result::numericComparisonFailed } };

    auto parsed = RoundTrip(completion);

    EXPECT_EQ(gap::peripheral::PairingStatus::Result::numericComparisonFailed, parsed.result.result);
}

TEST(GapProtoTest, round_trip_bond_completion)
{
    gap::central::BondCompletion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::busy } };

    EXPECT_EQ(gap::central::RequestStatus::Status::busy, RoundTrip(completion).requestStatus.status);
}

TEST(GapProtoTest, round_trip_discovered_device_reports_advertising_event_type)
{
    std::array<uint8_t, 3> advertisingData{ 0x02, 0x01, 0x06 };
    gap::central::DiscoveredDevice device{
        gap::central::Address{ infra::MakeRange(std::array<uint8_t, 6>{ 0, 1, 2, 3, 4, 5 }) },
        gap::central::AddressType{ gap::central::AddressType::AddressTypeEnum::randomAddress },
        infra::MakeRange(advertisingData),
        -75,
        gap::central::AdvertisingEventType{ gap::central::AdvertisingEventType::AdvertisingEventTypeEnum::advDirectInd }
    };

    auto parsed = RoundTrip(device);

    EXPECT_EQ(gap::central::AdvertisingEventType::AdvertisingEventTypeEnum::advDirectInd, parsed.eventType.type);
    EXPECT_EQ(gap::central::AddressType::AddressTypeEnum::randomAddress, parsed.addressType.type);
    EXPECT_EQ(-75, parsed.rssi);
}

TEST(GapProtoTest, round_trip_device_address)
{
    gap::central::DeviceAddress deviceAddress{
        gap::central::Address{ infra::MakeRange(std::array<uint8_t, 6>{ 5, 4, 3, 2, 1, 0 }) },
        gap::central::AddressType{ gap::central::AddressType::AddressTypeEnum::publicAddress }
    };

    EXPECT_EQ(deviceAddress, RoundTrip(deviceAddress));
}
