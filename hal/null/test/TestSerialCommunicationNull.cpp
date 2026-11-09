#include "hal/null/SerialCommunicationNull.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(SerialCommunicationNullTest, SendData_completes)
{
    hal::SerialCommunicationNull serial;
    testing::StrictMock<infra::MockCallback<void()>> onCompletion;

    EXPECT_CALL(onCompletion, callback());
    serial.SendData(infra::MakeStringByteRange("data"), [&onCompletion]
        {
            onCompletion.callback();
        });
}

TEST(SerialCommunicationNullTest, ReceiveData_never_reports_data)
{
    hal::SerialCommunicationNull serial;
    testing::StrictMock<infra::MockCallback<void(infra::ConstByteRange)>> onReceived;

    serial.ReceiveData([&onReceived](infra::ConstByteRange data)
        {
            onReceived.callback(data);
        });
    serial.SendData(infra::MakeStringByteRange("data"), [] {});
}
