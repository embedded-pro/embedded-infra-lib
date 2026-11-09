#include "hal/null/CanNull.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <initializer_list>

TEST(CanNullTest, SendData_completes_successfully)
{
    hal::CanNull can;
    testing::StrictMock<infra::MockCallback<void(bool)>> onCompletion;

    EXPECT_CALL(onCompletion, callback(true));
    can.SendData(hal::Can::Id::Create11BitId(0x123), hal::Can::Message{ std::initializer_list<uint8_t>{ 1, 2, 3 } }, [&onCompletion](bool success)
        {
            onCompletion.callback(success);
        });
}

TEST(CanNullTest, ReceiveData_never_reports_a_frame)
{
    hal::CanNull can;
    testing::StrictMock<infra::MockCallback<void(hal::Can::Id)>> onReceived;

    can.ReceiveData([&onReceived](hal::Can::Id id, const hal::Can::Message&)
        {
            onReceived.callback(id);
        });
    can.SendData(hal::Can::Id::Create29BitId(0x1234), hal::Can::Message{}, [](bool) {});
}
