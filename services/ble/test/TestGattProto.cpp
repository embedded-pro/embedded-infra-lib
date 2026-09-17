#include "generated/echo/GattClient.pb.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/stream/ByteOutputStream.hpp"
#include "infra/syntax/ProtoFormatter.hpp"
#include "infra/syntax/ProtoParser.hpp"
#include "services/ble/GattTypes.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    template<class Message>
    Message RoundTrip(const Message& message)
    {
        infra::ByteOutputStream::WithStorage<1024> stream;
        infra::ProtoFormatter formatter(stream);
        message.Serialize(formatter);

        infra::ByteInputStream inputStream(stream.Writer().Processed());
        infra::ProtoParser parser(inputStream);

        return Message(parser);
    }

    gatt::client::ConnectionId ConnectionId(uint32_t value)
    {
        return gatt::client::ConnectionId{ value };
    }
}

TEST(GattProtoTest, round_trip_accepted_completion)
{
    gatt::client::Completion completion{ ConnectionId(3), gatt::client::RequestStatus{ gatt::client::RequestStatus::Status::accepted }, gatt::client::Completion::Result::success };

    EXPECT_EQ(completion, RoundTrip(completion));
}

TEST(GattProtoTest, round_trip_rejected_completion)
{
    gatt::client::Completion completion{ ConnectionId(1), gatt::client::RequestStatus{ gatt::client::RequestStatus::Status::busy }, gatt::client::Completion::Result::success };

    auto parsed = RoundTrip(completion);

    EXPECT_EQ(1, parsed.connection.value);
    EXPECT_EQ(gatt::client::RequestStatus::Status::busy, parsed.requestStatus.status);
    EXPECT_EQ(gatt::client::Completion::Result::success, parsed.result);
}

TEST(GattProtoTest, request_status_carries_every_gatt_request_status)
{
    for (auto status : { gatt::client::RequestStatus::Status::accepted, gatt::client::RequestStatus::Status::invalidState,
             gatt::client::RequestStatus::Status::invalidParameter, gatt::client::RequestStatus::Status::busy,
             gatt::client::RequestStatus::Status::notSupported })
    {
        gatt::client::RequestStatus requestStatus{ status };
        EXPECT_EQ(requestStatus, RoundTrip(requestStatus));
    }

    EXPECT_EQ(static_cast<uint32_t>(services::GattRequestStatus::accepted), static_cast<uint32_t>(gatt::client::RequestStatus::Status::accepted));
    EXPECT_EQ(static_cast<uint32_t>(services::GattRequestStatus::invalidState), static_cast<uint32_t>(gatt::client::RequestStatus::Status::invalidState));
    EXPECT_EQ(static_cast<uint32_t>(services::GattRequestStatus::invalidParameter), static_cast<uint32_t>(gatt::client::RequestStatus::Status::invalidParameter));
    EXPECT_EQ(static_cast<uint32_t>(services::GattRequestStatus::busy), static_cast<uint32_t>(gatt::client::RequestStatus::Status::busy));
    EXPECT_EQ(static_cast<uint32_t>(services::GattRequestStatus::notSupported), static_cast<uint32_t>(gatt::client::RequestStatus::Status::notSupported));
}

TEST(GattProtoTest, completion_carries_every_gatt_result)
{
    for (auto result : { gatt::client::Completion::Result::success, gatt::client::Completion::Result::invalidHandle,
             gatt::client::Completion::Result::notPermitted, gatt::client::Completion::Result::insufficientAuthentication,
             gatt::client::Completion::Result::insufficientAuthorization, gatt::client::Completion::Result::insufficientEncryption,
             gatt::client::Completion::Result::insufficientResources, gatt::client::Completion::Result::invalidLength,
             gatt::client::Completion::Result::unsupported, gatt::client::Completion::Result::disconnected,
             gatt::client::Completion::Result::timeout, gatt::client::Completion::Result::unknown,
             gatt::client::Completion::Result::databaseOutOfSync, gatt::client::Completion::Result::valueNotAllowed })
    {
        gatt::client::Completion completion{ ConnectionId(7), gatt::client::RequestStatus{ gatt::client::RequestStatus::Status::accepted }, result };
        EXPECT_EQ(completion, RoundTrip(completion));
    }

    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::success), static_cast<uint32_t>(gatt::client::Completion::Result::success));
    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::invalidHandle), static_cast<uint32_t>(gatt::client::Completion::Result::invalidHandle));
    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::disconnected), static_cast<uint32_t>(gatt::client::Completion::Result::disconnected));
    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::unknown), static_cast<uint32_t>(gatt::client::Completion::Result::unknown));
    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::databaseOutOfSync), static_cast<uint32_t>(gatt::client::Completion::Result::databaseOutOfSync));
    EXPECT_EQ(static_cast<uint32_t>(services::GattResult::valueNotAllowed), static_cast<uint32_t>(gatt::client::Completion::Result::valueNotAllowed));
}

TEST(GattProtoTest, the_established_completion_results_did_not_move)
{
    // Pins the values that existed before databaseOutOfSync and valueNotAllowed were appended.
    // Inserting a value rather than appending one would renumber everything after it and break
    // every port silently, so this fails loudly if anyone tries.
    EXPECT_EQ(0u, static_cast<uint32_t>(services::GattResult::success));
    EXPECT_EQ(11u, static_cast<uint32_t>(services::GattResult::unknown));
    EXPECT_EQ(12u, static_cast<uint32_t>(services::GattResult::databaseOutOfSync));
    EXPECT_EQ(13u, static_cast<uint32_t>(services::GattResult::valueNotAllowed));
}

TEST(GattProtoTest, round_trip_characteristic_data_carries_its_connection)
{
    const std::array<uint8_t, 4> storage{ 1, 2, 3, 4 };
    gatt::client::CharacteristicData data{ ConnectionId(2), gatt::client::Handle{ 0x21 }, infra::MakeRange(storage) };

    auto parsed = RoundTrip(data);

    EXPECT_EQ(2, parsed.connection.value);
    EXPECT_EQ(0x21, parsed.handle.value);
    EXPECT_EQ(data.data, parsed.data);
}

TEST(GattProtoTest, round_trip_handle_range_carries_its_connection)
{
    gatt::client::HandleRange range{ ConnectionId(5), gatt::client::Handle{ 0x1 }, gatt::client::Handle{ 0x9 } };

    EXPECT_EQ(range, RoundTrip(range));
}

TEST(GattProtoTest, round_trip_read_completion)
{
    const std::array<uint8_t, 2> storage{ 0xaa, 0xbb };
    gatt::client::ReadCompletion readCompletion{
        gatt::client::Completion{ ConnectionId(4), gatt::client::RequestStatus{ gatt::client::RequestStatus::Status::accepted }, gatt::client::Completion::Result::success },
        infra::MakeRange(storage)
    };

    auto parsed = RoundTrip(readCompletion);

    EXPECT_EQ(4, parsed.completion.connection.value);
    EXPECT_EQ(gatt::client::Completion::Result::success, parsed.completion.result);
    EXPECT_EQ(readCompletion.data, parsed.data);
}

TEST(GattProtoTest, the_long_operations_take_new_method_ids)
{
    // A changed payload moves to a new id and the old id is left unused, so a peer speaking an
    // older version fails on an unknown method rather than misreading a known one. These four
    // are additions, so they simply continue past the previous maxima of 27 and 33.
    EXPECT_EQ(28u, gatt::client::GattClientProxy::idReadLong);
    EXPECT_EQ(29u, gatt::client::GattClientProxy::idWriteLong);
    EXPECT_EQ(34u, gatt::client::GattClientResponseProxy::idReadLongComplete);
    EXPECT_EQ(35u, gatt::client::GattClientResponseProxy::idWriteLongComplete);
}

TEST(GattProtoTest, round_trip_a_long_read_completion)
{
    std::array<uint8_t, 300> storage{};
    for (std::size_t i = 0; i != storage.size(); ++i)
        storage[i] = static_cast<uint8_t>(i);

    gatt::client::ReadCompletion completion{
        gatt::client::Completion{ ConnectionId(3), gatt::client::RequestStatus{ gatt::client::RequestStatus::Status::accepted }, gatt::client::Completion::Result::success },
        infra::MakeRange(storage)
    };

    infra::ByteOutputStream::WithStorage<512> stream;
    infra::ProtoFormatter formatter(stream);
    completion.Serialize(formatter);

    infra::ByteInputStream inputStream(stream.Writer().Processed());
    infra::ProtoParser parser(inputStream);
    gatt::client::ReadCompletion parsed(parser);

    EXPECT_EQ(3, parsed.completion.connection.value);
    EXPECT_EQ(gatt::client::Completion::Result::success, parsed.completion.result);
    EXPECT_EQ(completion.data, parsed.data);
}
