#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/SharedPtr.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include <vector>

class I2cRegisterAccessTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    static const hal::I2cAddress slaveAddress;

    I2cRegisterAccessTest()
        : master()
        , registerAccess(master, slaveAddress)
    {}

    hal::I2cMasterMock master;
    hal::I2cMasterRegisterAccessByte registerAccess;
};

const hal::I2cAddress I2cRegisterAccessTest::slaveAddress(0x5a);

TEST_F(I2cRegisterAccessTest, TestReadRegister)
{
    infra::MockCallback<void()> callback;
    EXPECT_CALL(callback, callback());
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::repeatedStart, std::vector<uint8_t>{ 3 }));
    EXPECT_CALL(master, ReceiveDataMock(slaveAddress, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 5, 6, 7 }));

    std::array<uint8_t, 3> data;
    registerAccess.ReadRegister(3, infra::MakeByteRange(data), [&callback]()
        {
            callback.callback();
        });

    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 3>{ 5, 6, 7 }), data);
}

TEST_F(I2cRegisterAccessTest, TestWriteRegister)
{
    infra::MockCallback<void()> callback;
    EXPECT_CALL(callback, callback());
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::continueSession, std::vector<uint8_t>{ 3 }));
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::stop, std::vector<uint8_t>{ 5, 6, 7 }));

    std::array<uint8_t, 3> data{ 5, 6, 7 };
    registerAccess.WriteRegister(3, infra::MakeByteRange(data), [&callback]()
        {
            callback.callback();
        });

    ExecuteAllActions();
}

TEST_F(I2cRegisterAccessTest, the_completion_of_a_read_is_released_once_it_has_been_called)
{
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::repeatedStart, std::vector<uint8_t>{ 3 }));
    EXPECT_CALL(master, ReceiveDataMock(slaveAddress, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 5 }));
    infra::AccessedBySharedPtr access{ infra::emptyFunction };

    int object{ 0 };
    std::array<uint8_t, 1> data;

    registerAccess.ReadRegister(3, infra::MakeByteRange(data), [reference = access.MakeShared(object)]() {});
    ExecuteAllActions();

    EXPECT_FALSE(access.Referenced());
}

TEST_F(I2cRegisterAccessTest, the_completion_of_a_write_is_released_once_it_has_been_called)
{
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::continueSession, std::vector<uint8_t>{ 3 }));
    EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::stop, std::vector<uint8_t>{ 5 }));
    infra::AccessedBySharedPtr access{ infra::emptyFunction };

    int object{ 0 };
    std::array<uint8_t, 1> data{ 5 };

    registerAccess.WriteRegister(3, infra::MakeByteRange(data), [reference = access.MakeShared(object)]() {});
    ExecuteAllActions();

    EXPECT_FALSE(access.Referenced());
}

class I2cRegisterAccessHalfWordTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    static const hal::I2cAddress slaveAddress;

    void ExpectRead(std::vector<uint8_t> registerBytes)
    {
        testing::InSequence sequence;
        EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::repeatedStart, registerBytes));
        EXPECT_CALL(master, ReceiveDataMock(slaveAddress, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 5, 6 }));
    }

    void ExpectWrite(std::vector<uint8_t> registerBytes)
    {
        testing::InSequence sequence;
        EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::continueSession, registerBytes));
        EXPECT_CALL(master, SendDataMock(slaveAddress, hal::Action::stop, std::vector<uint8_t>{ 5, 6, 7 }));
    }

    testing::StrictMock<hal::I2cMasterMock> master;
    hal::I2cMasterRegisterAccessHalfWordBigEndian bigEndian{ master, slaveAddress };
    hal::I2cMasterRegisterAccessHalfWordLittleEndian littleEndian{ master, slaveAddress };
    testing::StrictMock<infra::MockCallback<void()>> callback;
    std::array<uint8_t, 2> readData{};
};

const hal::I2cAddress I2cRegisterAccessHalfWordTest::slaveAddress(0x1a);

TEST_F(I2cRegisterAccessHalfWordTest, big_endian_read_sends_the_most_significant_byte_of_the_register_first)
{
    ExpectRead({ 0x01, 0x02 });
    EXPECT_CALL(callback, callback());

    bigEndian.ReadRegister(0x0102, infra::MakeByteRange(readData), [this]()
        {
            callback.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 2>{ 5, 6 }), readData);
}

TEST_F(I2cRegisterAccessHalfWordTest, big_endian_write_sends_the_most_significant_byte_of_the_register_first)
{
    ExpectWrite({ 0x01, 0x02 });
    EXPECT_CALL(callback, callback());
    std::array<uint8_t, 3> data{ 5, 6, 7 };

    bigEndian.WriteRegister(0x0102, infra::MakeByteRange(data), [this]()
        {
            callback.callback();
        });
    ExecuteAllActions();
}

TEST_F(I2cRegisterAccessHalfWordTest, little_endian_read_sends_the_least_significant_byte_of_the_register_first)
{
    ExpectRead({ 0x02, 0x01 });
    EXPECT_CALL(callback, callback());

    littleEndian.ReadRegister(0x0102, infra::MakeByteRange(readData), [this]()
        {
            callback.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 2>{ 5, 6 }), readData);
}

TEST_F(I2cRegisterAccessHalfWordTest, little_endian_write_sends_the_least_significant_byte_of_the_register_first)
{
    ExpectWrite({ 0x02, 0x01 });
    EXPECT_CALL(callback, callback());
    std::array<uint8_t, 3> data{ 5, 6, 7 };

    littleEndian.WriteRegister(0x0102, infra::MakeByteRange(data), [this]()
        {
            callback.callback();
        });
    ExecuteAllActions();
}
