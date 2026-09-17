#include "drivers/imu/lsm303dlhc/Lsm303dlhcAccelerometerWithFifo.hpp"
#include "drivers/imu/lsm303dlhc/Lsm303dlhcMagnetometer.hpp"
#include "drivers/imu/lsm303dlhc/Lsm303dlhcWithPolling.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <vector>

namespace
{
    using AccelerometerCore = drivers::Lsm303dlhcAccelerometer;
    using MagnetometerCore = drivers::Lsm303dlhcMagnetometer;
    using FifoDevice = drivers::Lsm303dlhcAccelerometerWithFifo<AccelerometerCore>;
    using PolledAccelerometer = drivers::Lsm303dlhcWithPolling<AccelerometerCore>;
    using PolledMagnetometer = drivers::Lsm303dlhcWithPolling<MagnetometerCore>;
    using PolledFifoDevice = drivers::Lsm303dlhcAccelerometerWithFifo<PolledAccelerometer>;

    template<class Device>
    class Lsm303dlhcAccelerometerMixinFixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Initialize()
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));
            EXPECT_CALL(bus, ReadRegisterMock(0x20, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x57 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x88 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x26, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(typename AccelerometerCore::Config(), [](AccelerometerCore::InitializationResult) {});

            this->ForwardTime(std::chrono::milliseconds(7));
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x10 }));

            device.AsAccelerometer().Start([this](AccelerometerCore::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        acceleration.push_back(sample.Value());
                });

            this->ExecuteAllActions();
        }

        static std::vector<uint8_t> Frames(const std::vector<int16_t>& counts)
        {
            std::vector<uint8_t> data;

            for (int16_t count : counts)
            {
                auto word = static_cast<uint16_t>(static_cast<uint16_t>(count) << 4);
                data.push_back(static_cast<uint8_t>(word & 0xff));
                data.push_back(static_cast<uint8_t>(word >> 8));
            }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::vector<int32_t> acceleration;
    };

    class Lsm303dlhcWithFifoTest
        : public Lsm303dlhcAccelerometerMixinFixture<FifoDevice>
    {
    public:
        void EnableFifo(const FifoDevice::FifoConfig& fifoConfig = FifoDevice::FifoConfig())
        {
            testing::InSequence sequence;

            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            ExpectModifyRegister(0x24, 0x00, 0x40);
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x90 }));
            ExpectModifyRegister(0x22, 0x00, 0x06);

            infra::VerifyingFunction<void()> done;
            device.EnableFifo(fifoConfig, done);

            ExecuteAllActions();
        }
    };

    class Lsm303dlhcAccelerometerWithPollingTest
        : public Lsm303dlhcAccelerometerMixinFixture<PolledAccelerometer>
    {};

    class Lsm303dlhcCompositionTest
        : public Lsm303dlhcAccelerometerMixinFixture<PolledFifoDevice>
    {};

    class Lsm303dlhcMagnetometerWithPollingTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Initialize()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x0a, 3)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x48, 0x34, 0x33 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x00, std::vector<uint8_t>{ 0x90 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x20 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x00 }));

            device.Initialize(MagnetometerCore::Config(), [](MagnetometerCore::InitializationResult) {});

            ForwardTime(std::chrono::milliseconds(6));
        }

        void StartStreaming()
        {
            device.AsMagnetometer().Start([this](MagnetometerCore::Magnetometer::Samples samples)
                {
                    for (auto sample : samples)
                        magneticFluxDensity.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        PolledMagnetometer device{ bus };
        std::vector<int32_t> magneticFluxDensity;
    };
}

TEST_F(Lsm303dlhcWithFifoTest, enable_fifo_passes_through_bypass_before_selecting_the_mode)
{
    Initialize();
    EnableFifo();
}

TEST_F(Lsm303dlhcWithFifoTest, enable_fifo_programs_the_configured_mode_and_watermark)
{
    Initialize();

    FifoDevice::FifoConfig fifoConfig;
    fifoConfig.mode = FifoDevice::FifoMode::fifo;
    fifoConfig.watermark = 8;
    fifoConfig.interruptOnOverrun = false;

    testing::InSequence sequence;

    EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
    ExpectModifyRegister(0x24, 0x00, 0x40);
    EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x48 }));
    ExpectModifyRegister(0x22, 0x00, 0x04);

    infra::VerifyingFunction<void()> done;
    device.EnableFifo(fifoConfig, done);

    ExecuteAllActions();
}

TEST_F(Lsm303dlhcWithFifoTest, a_data_ready_interrupt_drains_every_stored_frame)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x02 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x28, 12)).WillOnce(testing::Return(Frames({ 1000, 0, 0, 0, 1000, 0 })));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    ASSERT_EQ(6u, acceleration.size());
    EXPECT_EQ(9807, acceleration[0]);
    EXPECT_EQ(9807, acceleration[4]);
}

TEST_F(Lsm303dlhcWithFifoTest, nothing_is_delivered_when_the_buffer_is_empty)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x20 }));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Lsm303dlhcWithFifoTest, a_drain_larger_than_the_batch_buffer_is_split_over_several_reads)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0a }));
    EXPECT_CALL(bus, ReadRegisterMock(0x28, 48)).WillOnce(testing::Return(Frames(std::vector<int16_t>(24, 1000))));
    EXPECT_CALL(bus, ReadRegisterMock(0x28, 12)).WillOnce(testing::Return(Frames(std::vector<int16_t>(6, 1000))));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(30u, acceleration.size());
}

TEST_F(Lsm303dlhcWithFifoTest, an_overrun_passes_the_buffer_through_bypass_and_reports_it)
{
    Initialize();
    EnableFifo();
    StartStreaming();

    bool overrun = false;
    device.OnOverrun([&overrun]()
        {
            overrun = true;
        });

    EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x40 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x90 }));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_TRUE(overrun);
    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Lsm303dlhcWithFifoTest, disabling_the_fifo_restores_direct_measurement_reads)
{
    Initialize();
    EnableFifo();

    {
        testing::InSequence sequence;

        ExpectModifyRegister(0x22, 0x06, 0x00);
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        ExpectModifyRegister(0x24, 0x40, 0x00);

        infra::VerifyingFunction<void()> done;
        device.DisableFifo(done);

        ExecuteAllActions();
    }

    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames({ 1000, 0, 0 })));

    dataReadyPin.SetStubState(true);
    ExecuteAllActions();

    EXPECT_EQ(3u, acceleration.size());
}

TEST_F(Lsm303dlhcAccelerometerWithPollingTest, the_accelerometer_status_register_drives_the_reads)
{
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames({ 1000, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(5));

    EXPECT_EQ(3u, acceleration.size());
}

TEST_F(Lsm303dlhcAccelerometerWithPollingTest, a_tick_without_new_data_reads_nothing)
{
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    ForwardTime(std::chrono::milliseconds(5));

    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Lsm303dlhcAccelerometerWithPollingTest, the_status_read_is_skipped_when_verification_is_disabled)
{
    Initialize();
    device.SetVerifyDataReady(false);
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames({ 1000, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(5));

    EXPECT_EQ(3u, acceleration.size());
}

TEST_F(Lsm303dlhcAccelerometerWithPollingTest, the_polling_interval_is_configurable)
{
    Initialize();
    device.SetPollingInterval(std::chrono::milliseconds(20));
    StartStreaming();

    ForwardTime(std::chrono::milliseconds(19));

    EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(Lsm303dlhcAccelerometerWithPollingTest, stopping_cancels_the_poll_timer)
{
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));

    device.AsAccelerometer().Stop();
    ExecuteAllActions();

    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_TRUE(acceleration.empty());
}

TEST_F(Lsm303dlhcMagnetometerWithPollingTest, the_magnetometer_status_register_drives_the_reads)
{
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x09, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x03, 6)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x04, 0x4c, 0x00, 0x00, 0x00, 0x00 }));

    ForwardTime(std::chrono::milliseconds(5));

    ASSERT_EQ(3u, magneticFluxDensity.size());
    EXPECT_EQ(1000, magneticFluxDensity[0]);
}

TEST_F(Lsm303dlhcMagnetometerWithPollingTest, a_tick_without_new_data_reads_nothing)
{
    Initialize();
    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x09, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

    ForwardTime(std::chrono::milliseconds(5));

    EXPECT_TRUE(magneticFluxDensity.empty());
}

TEST_F(Lsm303dlhcCompositionTest, fifo_over_polling_drains_the_buffer_on_a_timer_tick)
{
    Initialize();

    {
        testing::InSequence sequence;

        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        ExpectModifyRegister(0x24, 0x00, 0x40);
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x90 }));
        ExpectModifyRegister(0x22, 0x00, 0x06);

        infra::VerifyingFunction<void()> done;
        device.EnableFifo(PolledFifoDevice::FifoConfig(), done);

        ExecuteAllActions();
    }

    StartStreaming();

    EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }));
    EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames({ 1000, 0, 0 })));

    ForwardTime(std::chrono::milliseconds(5));

    EXPECT_EQ(3u, acceleration.size());
}
