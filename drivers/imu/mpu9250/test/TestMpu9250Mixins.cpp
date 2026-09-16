#include "drivers/imu/mpu9250/Mpu9250WithFifo.hpp"
#include "drivers/imu/mpu9250/Mpu9250WithPolling.hpp"
#include "drivers/imu/mpu9250/Mpu9250WithSelfTest.hpp"
#include "drivers/imu/mpu9250/test/Mpu9250BusAccessMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using InitializationResult = drivers::Mpu9250Core::InitializationResult;

    template<class Device>
    class Mpu9250MixinFixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Mpu9250MixinFixture()
        {
            EXPECT_CALL(bus, RequiresI2cSlaveInterfaceDisabled()).WillRepeatedly(testing::Return(false));
        }

        void Initialize()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x75, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x71 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x01 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x6c, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x19, std::vector<uint8_t>{ 0x04 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1a, std::vector<uint8_t>{ 0x03 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1b, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1c, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x1d, std::vector<uint8_t>{ 0x03 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x37, std::vector<uint8_t>{ 0x10 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(drivers::Mpu9250Core::Config(), [](InitializationResult) {});

            this->ForwardTime(std::chrono::milliseconds(101));
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        static std::vector<uint8_t> Frames(const std::vector<int16_t>& words)
        {
            std::vector<uint8_t> data;

            for (int16_t word : words)
            {
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(word) >> 8));
                data.push_back(static_cast<uint8_t>(word & 0xff));
            }

            return data;
        }

        testing::StrictMock<drivers::Mpu9250BusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::vector<int32_t> acceleration;
        std::vector<int32_t> angularVelocity;
    };

    class Mpu9250WithFifoTest
        : public Mpu9250MixinFixture<drivers::Mpu9250WithFifo<drivers::Mpu9250Core>>
    {
    public:
        void EnableFifo(bool stopWhenFull = true)
        {
            testing::InSequence sequence;

            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x78 }));
            ExpectModifyRegister(0x1a, 0x03, stopWhenFull ? 0x43 : 0x03);
            ExpectModifyRegister(0x6a, 0x00, 0x04);
            ExpectModifyRegister(0x6a, 0x00, 0x40);

            drivers::Mpu9250WithFifo<drivers::Mpu9250Core>::FifoConfig fifoConfig;
            fifoConfig.stopWhenFull = stopWhenFull;

            infra::VerifyingFunction<void()> done;
            device.EnableFifo(fifoConfig, done);

            ExecuteAllActions();
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

            device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        acceleration.push_back(sample.Value());
                });
            device.AsGyroscope().Start([this](drivers::Mpu9250Core::Gyroscope::Samples samples)
                {
                    for (auto sample : samples)
                        angularVelocity.push_back(sample.Value());
                });

            ExecuteAllActions();
        }
    };

    class Mpu9250WithPollingTest
        : public Mpu9250MixinFixture<drivers::Mpu9250WithPolling<drivers::Mpu9250Core>>
    {};

    using PolledFifoDevice = drivers::Mpu9250WithFifo<drivers::Mpu9250WithPolling<drivers::Mpu9250Core>>;

    class Mpu9250CompositionTest
        : public Mpu9250MixinFixture<PolledFifoDevice>
    {};
}

TEST_F(Mpu9250WithFifoTest, enable_fifo_selects_the_accelerometer_and_gyroscope_and_resets_the_buffer)
{
    Initialize();
    EnableFifo();
}

TEST_F(Mpu9250WithFifoTest, enable_fifo_leaves_fifo_mode_clear_when_overwriting_the_oldest_sample)
{
    Initialize();
    EnableFifo(false);
}

TEST_F(Mpu9250WithFifoTest, enable_fifo_selects_only_the_requested_sensors)
{
    Initialize();

    testing::InSequence sequence;

    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));
    ExpectModifyRegister(0x1a, 0x03, 0x43);
    ExpectModifyRegister(0x6a, 0x00, 0x04);
    ExpectModifyRegister(0x6a, 0x00, 0x40);

    drivers::Mpu9250WithFifo<drivers::Mpu9250Core>::FifoConfig fifoConfig;
    fifoConfig.gyroscope = false;
    fifoConfig.temperature = true;

    infra::VerifyingFunction<void()> done;
    device.EnableFifo(fifoConfig, done);

    ExecuteAllActions();
}

TEST_F(Mpu9250WithFifoTest, a_data_ready_interrupt_drains_every_complete_frame)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x72, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x18 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x74, 24)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 16384, 0, 0, 0, 16384, 0, 0, 16384, 0 })));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(6u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
    EXPECT_EQ(0, acceleration[1]);
    EXPECT_EQ(9807, acceleration[4]);

    ASSERT_EQ(6u, angularVelocity.size());
    EXPECT_EQ(125000, angularVelocity[0]);
    EXPECT_EQ(125000, angularVelocity[4]);
}

TEST_F(Mpu9250WithFifoTest, a_partial_trailing_frame_is_left_in_the_buffer)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x72, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x11 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x74, 12)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 0, 0, 0 })));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(3u, acceleration.size());
}

TEST_F(Mpu9250WithFifoTest, nothing_is_delivered_when_the_buffer_holds_less_than_one_frame)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x72, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x08 }));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(acceleration.empty());
    EXPECT_TRUE(angularVelocity.empty());
}

TEST_F(Mpu9250WithFifoTest, a_drain_larger_than_the_batch_buffer_is_split_over_several_reads)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x72, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x78 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x74, 96)).WillOnce(testing::Return(std::vector<uint8_t>(96, 0)));
    EXPECT_CALL(bus, ReadRegisterMock(0x74, 24)).WillOnce(testing::Return(std::vector<uint8_t>(24, 0)));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(30u, acceleration.size());
}

TEST_F(Mpu9250WithFifoTest, an_overflow_resets_the_buffer_and_reports_it)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    infra::MockCallback<void()> overflow;
    device.OnOverflow([&overflow]()
        {
            overflow.callback();
        });

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x11 }));
    ExpectModifyRegister(0x6a, 0x40, 0x44);
    EXPECT_CALL(overflow, callback());

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Mpu9250WithFifoTest, disabling_the_fifo_restores_direct_measurement_reads)
{
    Initialize();
    EnableFifo();

    ExpectModifyRegister(0x6a, 0x40, 0x00);
    EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x00 }));

    infra::VerifyingFunction<void()> done;
    device.DisableFifo(done);
    ExecuteAllActions();

    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 0, 0, 0, 0 })));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(3u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
}

TEST_F(Mpu9250WithPollingTest, no_gpio_interrupt_is_used_and_the_timer_drives_the_reads)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.SetPollingInterval(std::chrono::milliseconds(10));
    device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                acceleration.push_back(sample.Value());
        });

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(acceleration.empty());

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 0, 0, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(10));

    ASSERT_EQ(3u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
}

TEST_F(Mpu9250WithPollingTest, a_tick_without_the_data_ready_flag_reads_nothing)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.SetPollingInterval(std::chrono::milliseconds(10));
    device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                acceleration.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Mpu9250WithPollingTest, the_status_read_is_skipped_when_verification_is_disabled)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.SetPollingInterval(std::chrono::milliseconds(10));
    device.SetVerifyDataReady(false);
    device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                acceleration.push_back(sample.Value());
        });

    ExecuteAllActions();

    EXPECT_CALL(bus, ReadRegisterMock(0x3b, 14)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 0, 0, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_EQ(3u, acceleration.size());
}

TEST_F(Mpu9250WithPollingTest, stopping_cancels_the_poll_timer)
{
    Initialize();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.SetPollingInterval(std::chrono::milliseconds(10));
    device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples) {});

    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x00 }));

    device.AsAccelerometer().Stop();
    ExecuteAllActions();

    ForwardTime(std::chrono::milliseconds(50));
}

TEST_F(Mpu9250CompositionTest, fifo_over_polling_drains_the_buffer_on_a_timer_tick)
{
    Initialize();

    {
        testing::InSequence sequence;

        EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x78 }));
        ExpectModifyRegister(0x1a, 0x03, 0x43);
        ExpectModifyRegister(0x6a, 0x00, 0x04);
        ExpectModifyRegister(0x6a, 0x00, 0x40);
    }

    infra::VerifyingFunction<void()> fifoEnabled;
    device.EnableFifo(PolledFifoDevice::FifoConfig(), fifoEnabled);
    ExecuteAllActions();

    EXPECT_CALL(bus, WriteRegisterMock(0x38, std::vector<uint8_t>{ 0x01 }));

    device.SetPollingInterval(std::chrono::milliseconds(10));
    device.AsAccelerometer().Start([this](drivers::Mpu9250Core::Accelerometer::Samples samples)
        {
            for (auto sample : samples)
                acceleration.push_back(sample.Value());
        });

    ExecuteAllActions();

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(acceleration.empty());

    EXPECT_CALL(bus, ReadRegisterMock(0x3a, 1)).Times(2).WillRepeatedly(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x72, 2)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00, 0x0c }));
    EXPECT_CALL(bus, ReadRegisterMock(0x74, 12)).WillOnce(testing::Return(Frames({ 16384, 0, 0, 0, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(10));

    ASSERT_EQ(3u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
}
