#include "hal/qemu/sync/SemihostingWriter.hpp"
#include "infra/stream/StreamErrorPolicy.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "gtest/gtest.h"

TEST(SemihostingWriterTest, inserts_bytes_to_sink)
{
    infra::BoundedString::WithStorage<64> captured;
    hal::SemihostingWriter writer([&](infra::ConstByteRange range)
        {
            captured.append(infra::ByteRangeAsString(range));
        });

    infra::StreamErrorPolicy policy{ infra::noFail };
    writer.Insert(infra::MakeStringByteRange("hello"), policy);

    EXPECT_EQ(captured, "hello");
}

TEST(SemihostingWriterTest, available_is_unbounded)
{
    hal::SemihostingWriter writer([](infra::ConstByteRange) {});
    EXPECT_EQ(writer.Available(), std::numeric_limits<std::size_t>::max());
}

TEST(SemihostingWriterTest, empty_range_does_not_call_sink)
{
    bool called = false;
    hal::SemihostingWriter writer([&](infra::ConstByteRange)
        {
            called = true;
        });
    infra::StreamErrorPolicy policy{ infra::noFail };
    writer.Insert(infra::ConstByteRange{}, policy);
    EXPECT_FALSE(called);
}

TEST(SemihostingWriterTest, multiple_inserts_append_in_order)
{
    infra::BoundedString::WithStorage<64> captured;
    hal::SemihostingWriter writer([&](infra::ConstByteRange range)
        {
            captured.append(infra::ByteRangeAsString(range));
        });
    infra::StreamErrorPolicy policy{ infra::noFail };
    writer.Insert(infra::MakeStringByteRange("foo"), policy);
    writer.Insert(infra::MakeStringByteRange("bar"), policy);
    EXPECT_EQ(captured, "foobar");
}
