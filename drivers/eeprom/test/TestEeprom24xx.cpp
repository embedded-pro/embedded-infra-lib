#include "drivers/eeprom/Eeprom24xx.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace
{
    using testing::ElementsAreArray;

    class TestEeprom24xx
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        infra::Function<void()> Done()
        {
            return [this]()
            {
                onDone.callback();
            };
        }

        void SendCompletes(hal::Result result = hal::Result::complete)
        {
            auto onSent = i2c.onSent;
            onSent(result, 0);
        }

        void ReceiveCompletes(hal::Result result = hal::Result::complete)
        {
            auto onReceived = i2c.onReceived;
            onReceived(result);
        }

        void ExpectPage(const hal::I2cAddress& device, uint8_t wordAddress, std::vector<uint8_t> data)
        {
            data.insert(data.begin(), wordAddress);
            EXPECT_CALL(i2c, SendDataMock(device, hal::Action::stop, data));
        }

        void ExpectPoll(const hal::I2cAddress& device, uint8_t wordAddress)
        {
            EXPECT_CALL(i2c, SendDataMock(device, hal::Action::stop, std::vector<uint8_t>{ wordAddress }));
        }

        const hal::I2cAddress block0{ 0x50 };
        const hal::I2cAddress block1{ 0x51 };
        testing::StrictMock<hal::I2cMasterMockWithoutAutomaticDone> i2c;
        testing::StrictMock<infra::MockCallback<void()>> onDone;
        drivers::Eeprom24xx eeprom{ i2c, drivers::Eeprom24xx::Config{} };
    };
}

TEST_F(TestEeprom24xx, reports_the_configured_size)
{
    EXPECT_EQ(eeprom.Size(), 512u);
}

TEST_F(TestEeprom24xx, reads_by_sending_the_word_address_then_receiving)
{
    std::array<uint8_t, 4> buffer{};

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x10 }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0x10, Done());

    EXPECT_CALL(i2c, ReceiveDataMock(block0, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4 }));
    SendCompletes();

    ReceiveCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();

    EXPECT_THAT(buffer, ElementsAreArray({ 1, 2, 3, 4 }));
}

TEST_F(TestEeprom24xx, a_read_across_a_block_boundary_selects_the_next_block_in_the_device_address)
{
    std::array<uint8_t, 4> buffer{};

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0xfe }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0xfe, Done());

    EXPECT_CALL(i2c, ReceiveDataMock(block0, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2 }));
    SendCompletes();

    EXPECT_CALL(i2c, SendDataMock(block1, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x00 }));
    ReceiveCompletes();

    EXPECT_CALL(i2c, ReceiveDataMock(block1, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 3, 4 }));
    SendCompletes();

    ReceiveCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();

    EXPECT_THAT(buffer, ElementsAreArray({ 1, 2, 3, 4 }));
}

TEST_F(TestEeprom24xx, an_unacknowledged_read_completes_with_erased_bytes)
{
    std::array<uint8_t, 3> buffer{ 1, 2, 3 };

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x00 }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0, Done());

    SendCompletes(hal::Result::partialComplete);

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();

    EXPECT_THAT(buffer, ElementsAreArray({ 0xff, 0xff, 0xff }));
}

TEST_F(TestEeprom24xx, a_read_ended_by_a_bus_error_completes_with_erased_bytes)
{
    std::array<uint8_t, 2> buffer{};

    EXPECT_CALL(i2c, SendDataMock(block0, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x20 }));
    eeprom.ReadBuffer(infra::MakeRange(buffer), 0x20, Done());

    EXPECT_CALL(i2c, ReceiveDataMock(block0, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 7, 8 }));
    SendCompletes();

    ReceiveCompletes(hal::Result::busError);

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();

    EXPECT_THAT(buffer, ElementsAreArray({ 0xff, 0xff }));
}

TEST_F(TestEeprom24xx, a_write_is_split_at_page_boundaries_and_each_page_waits_for_the_device_to_acknowledge_again)
{
    std::array<uint8_t, 20> data{};
    for (std::size_t i = 0; i != data.size(); ++i)
        data[i] = static_cast<uint8_t>(i);

    ExpectPage(block0, 0x0c, { 0, 1, 2, 3 });
    eeprom.WriteBuffer(infra::MakeRange(data), 0x0c, Done());

    ExpectPoll(block0, 0x0c);
    SendCompletes();

    ExpectPoll(block0, 0x0c);
    SendCompletes(hal::Result::partialComplete);

    ExpectPage(block0, 0x10, { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 });
    SendCompletes();

    ExpectPoll(block0, 0x10);
    SendCompletes();

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, a_write_to_the_second_block_selects_it_in_the_device_address)
{
    std::array<uint8_t, 2> data{ 0xaa, 0xbb };

    ExpectPage(block1, 0x02, { 0xaa, 0xbb });
    eeprom.WriteBuffer(infra::MakeRange(data), 0x102, Done());

    ExpectPoll(block1, 0x02);
    SendCompletes();

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, a_write_across_the_block_boundary_continues_in_the_next_block)
{
    std::array<uint8_t, 16> data{};
    data.fill(0x33);

    ExpectPage(block0, 0xf8, std::vector<uint8_t>(8, 0x33));
    eeprom.WriteBuffer(infra::MakeRange(data), 0xf8, Done());

    ExpectPoll(block0, 0xf8);
    SendCompletes();

    ExpectPage(block1, 0x00, std::vector<uint8_t>(8, 0x33));
    SendCompletes();

    ExpectPoll(block1, 0x00);
    SendCompletes();

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, an_unacknowledged_page_is_retried_until_the_write_timeout_and_then_skipped)
{
    std::array<uint8_t, 18> data{};
    data.fill(0x5a);

    ExpectPage(block0, 0x00, std::vector<uint8_t>(16, 0x5a));
    eeprom.WriteBuffer(infra::MakeRange(data), 0, Done());

    ExpectPage(block0, 0x00, std::vector<uint8_t>(16, 0x5a));
    SendCompletes(hal::Result::partialComplete);

    ForwardTime(std::chrono::milliseconds(10));

    ExpectPage(block0, 0x10, { 0x5a, 0x5a });
    SendCompletes(hal::Result::partialComplete);

    ExpectPoll(block0, 0x10);
    SendCompletes();

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, a_device_that_never_acknowledges_after_a_page_is_given_up_at_the_write_timeout)
{
    std::array<uint8_t, 1> data{ 0x01 };

    ExpectPage(block0, 0x40, { 0x01 });
    eeprom.WriteBuffer(infra::MakeRange(data), 0x40, Done());

    ExpectPoll(block0, 0x40);
    SendCompletes();

    ForwardTime(std::chrono::milliseconds(10));

    SendCompletes(hal::Result::partialComplete);

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, an_empty_write_completes_on_the_event_dispatcher)
{
    eeprom.WriteBuffer(infra::ConstByteRange(), 0x10, Done());

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}

TEST_F(TestEeprom24xx, erase_writes_the_erased_pattern_over_every_page)
{
    drivers::Eeprom24xx small{ i2c, drivers::Eeprom24xx::Config{ 0x50, 32, 16, std::chrono::milliseconds(10) } };

    ExpectPage(block0, 0x00, std::vector<uint8_t>(16, 0xff));
    small.Erase(Done());

    ExpectPoll(block0, 0x00);
    SendCompletes();

    ExpectPage(block0, 0x10, std::vector<uint8_t>(16, 0xff));
    SendCompletes();

    ExpectPoll(block0, 0x10);
    SendCompletes();

    SendCompletes();

    EXPECT_CALL(onDone, callback());
    ExecuteAllActions();
}
