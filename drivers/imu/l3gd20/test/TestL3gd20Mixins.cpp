#include "drivers/imu/l3gd20/L3gd20WithFifo.hpp"
#include "drivers/imu/l3gd20/L3gd20WithPolling.hpp"
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
    using Core = drivers::L3gd20Core;
    using Variant = Core::Variant;

    using FifoDevice = drivers::L3gd20WithFifo<Core>;
    using PolledDevice = drivers::L3gd20WithPolling<Core>;
    using PolledFifoDevice = drivers::L3gd20WithFifo<drivers::L3gd20WithPolling<Core>>;

    template<class Device>
    class L3gd20MixinFixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Initialize(Variant variant = Variant::l3gd20)
        {
            Core::Config config;
            config.variant = variant;
            config.turnOnTime = std::chrono::milliseconds(5);

            EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ variant == Variant::l3gd20h ? uint8_t{ 0xd7 } : uint8_t{ 0xd4 } }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));

            if (variant == Variant::l3gd20h)
                EXPECT_CALL(bus, WriteRegisterMock(0x39, std::vector<uint8_t>{ 0x00 }));

            EXPECT_CALL(bus, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

            this->device.Initialize(config, [](Core::InitializationResult) {});
            this->ForwardTime(std::chrono::milliseconds(30));
        }

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming(uint8_t control3 = 0x00)
        {
            ExpectModifyRegister(0x22, control3, static_cast<uint8_t>(control3 | 0x08));

            this->device.AsGyroscope().Start([this](Core::Gyroscope::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            this->ExecuteAllActions();
        }

        static std::vector<uint8_t> Frames(std::size_t count)
        {
            std::vector<uint8_t> data;

            for (std::size_t frame = 0; frame != count; ++frame)
                for (std::size_t axis = 0; axis != 3; ++axis)
                {
                    data.push_back(static_cast<uint8_t>(frame + axis));
                    data.push_back(0);
                }

            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        hal::GpioPinStub interruptPin;
        Device device{ bus, dataReadyPin, interruptPin };
        std::vector<int32_t> received;
    };

    class L3gd20FifoTest
        : public L3gd20MixinFixture<FifoDevice>
    {
    public:
        void EnableFifo(const FifoDevice::FifoConfig& fifoConfig = {}, uint8_t control5 = 0x00, uint8_t control5Result = 0x40, uint8_t control3 = 0x00, uint8_t control3Result = 0x06)
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            ExpectModifyRegister(0x24, control5, control5Result);
            EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ static_cast<uint8_t>((static_cast<uint8_t>(fifoConfig.mode) << 5) | (fifoConfig.watermark & 0x1f)) }));
            ExpectModifyRegister(0x22, control3, control3Result);

            infra::VerifyingFunction<void()> done;
            device.EnableFifo(fifoConfig, done);

            ExecuteAllActions();
        }
    };

    class L3gd20PollingTest
        : public L3gd20MixinFixture<PolledDevice>
    {};

    class L3gd20PolledFifoTest
        : public L3gd20MixinFixture<PolledFifoDevice>
    {};

    TEST_F(L3gd20FifoTest, enabling_the_buffer_passes_through_bypass_before_selecting_the_mode)
    {
        Initialize();
        EnableFifo();
    }

    TEST_F(L3gd20FifoTest, enabling_the_buffer_programs_the_configured_mode_and_watermark)
    {
        Initialize();

        FifoDevice::FifoConfig fifoConfig;
        fifoConfig.mode = FifoDevice::FifoMode::fifo;
        fifoConfig.watermark = 5;

        EnableFifo(fifoConfig);
    }

    // Data ready and the three buffer sources share CTRL_REG3, so arming one may not clear the others
    TEST_F(L3gd20FifoTest, starting_to_stream_preserves_the_buffer_interrupt_sources)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x20 }));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(L3gd20FifoTest, the_stop_on_watermark_bit_is_only_written_on_the_l3gd20h)
    {
        Initialize(Variant::l3gd20h);

        FifoDevice::FifoConfig fifoConfig;
        fifoConfig.stopOnWatermark = true;

        EnableFifo(fifoConfig, 0x00, 0x60);
    }

    TEST_F(L3gd20FifoTest, an_interrupt_drains_every_stored_frame)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x03 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 18)).WillOnce(testing::Return(Frames(3)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(9u, received.size());
    }

    TEST_F(L3gd20FifoTest, a_drain_larger_than_the_batch_buffer_is_split_over_several_reads)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0a }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 48)).WillOnce(testing::Return(Frames(8)));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 12)).WillOnce(testing::Return(Frames(2)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(30u, received.size());
    }

    TEST_F(L3gd20FifoTest, an_empty_buffer_delivers_nothing)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x20 }));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20FifoTest, an_overrun_passes_the_buffer_through_bypass_and_reports_it)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        infra::VerifyingFunction<void()> overrun;
        device.OnOverrun(overrun);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x5f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x50 }));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20FifoTest, an_overrun_without_a_registered_callback_still_restores_the_mode)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x5f }));
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x50 }));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(L3gd20FifoTest, an_overrun_recovery_issues_no_second_write_after_stop)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x5f }));
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        bus.CompletePending();

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);

        // The second FIFO control write would land on a stopped device; a strict mock fails if it does
        bus.CompletePending();
        ExecuteAllActions();
    }

    TEST_F(L3gd20FifoTest, a_drain_issues_no_further_batch_read_after_stop)
    {
        Initialize();
        EnableFifo();
        StartStreaming(0x06);

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x0a }));
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 48)).WillOnce(testing::Return(Frames(8)));
        bus.CompletePending();

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);

        // Two frames are still owed, but the trailing read must not reach a stopped device
        bus.CompletePending();
        ExecuteAllActions();
    }

    TEST_F(L3gd20FifoTest, disabling_the_buffer_restores_the_direct_measurement_read)
    {
        Initialize();
        EnableFifo();

        ExpectModifyRegister(0x22, 0x06, 0x00);
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        ExpectModifyRegister(0x24, 0x40, 0x00);

        infra::VerifyingFunction<void()> done;
        device.DisableFifo(done);
        ExecuteAllActions();

        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames(1)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_EQ(3u, received.size());
    }

    TEST_F(L3gd20PollingTest, the_status_register_drives_the_polled_reads)
    {
        Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames(1)));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_EQ(3u, received.size());
    }

    TEST_F(L3gd20PollingTest, a_tick_without_the_data_available_flag_reads_nothing)
    {
        Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20PollingTest, the_status_read_is_skipped_when_verification_is_disabled)
    {
        Initialize();
        device.SetVerifyDataReady(false);
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(Frames(1)));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_EQ(3u, received.size());
    }

    TEST_F(L3gd20PollingTest, the_polling_interval_is_configurable)
    {
        Initialize();
        device.SetPollingInterval(std::chrono::milliseconds(20));
        StartStreaming();

        ForwardTime(std::chrono::milliseconds(19));

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(2));
    }

    TEST_F(L3gd20PollingTest, a_tick_during_an_outstanding_transaction_is_skipped)
    {
        Initialize();
        StartStreaming();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
        ForwardTime(std::chrono::milliseconds(5));

        // The first read still owes its completion, so the next two ticks pass without touching the bus
        ForwardTime(std::chrono::milliseconds(10));

        EXPECT_TRUE(bus.CompletionPending());
        bus.CompletePending();
    }

    // The read half of a read-modify-write must count as an outstanding transaction, or a poll tick
    // starts a second transfer over the bus adapter's shared buffers
    TEST_F(L3gd20PollingTest, a_tick_during_the_read_of_a_read_modify_write_is_skipped)
    {
        Initialize();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        device.AsGyroscope().Start([](Core::Gyroscope::Samples) {});
        ExecuteAllActions();

        EXPECT_TRUE(bus.CompletionPending());

        // A strict mock fails the test if the tick read the status register anyway
        ForwardTime(std::chrono::milliseconds(15));

        EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));
        bus.CompletePending();
        bus.CompletePending();
        bus.completeAutomatically = true;
        ExecuteAllActions();
    }

    // Stop clears the callback while the status read is in flight, and invoking a cleared
    // infra::Function aborts rather than doing nothing
    TEST_F(L3gd20PollingTest, a_status_read_completing_after_stop_delivers_nothing)
    {
        Initialize();
        StartStreaming();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(bus.CompletionPending());

        bool stopped = false;
        device.Stop([&stopped]()
            {
                stopped = true;
            });

        bus.CompletePending();
        ExecuteAllActions();

        EXPECT_TRUE(stopped);
        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20PollingTest, stopping_cancels_the_poll_timer)
    {
        Initialize();
        StartStreaming();

        ExpectModifyRegister(0x22, 0x08, 0x00);

        device.AsGyroscope().Stop();
        ExecuteAllActions();

        ForwardTime(std::chrono::milliseconds(20));

        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20PolledFifoTest, the_buffer_is_drained_on_a_timer_tick)
    {
        Initialize();

        PolledFifoDevice::FifoConfig fifoConfig;
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
        ExpectModifyRegister(0x24, 0x00, 0x40);
        EXPECT_CALL(bus, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x50 }));
        ExpectModifyRegister(0x22, 0x00, 0x06);

        infra::VerifyingFunction<void()> configured;
        device.EnableFifo(fifoConfig, configured);
        ExecuteAllActions();

        StartStreaming(0x06);

        EXPECT_CALL(bus, ReadRegisterMock(0x27, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x2f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x02 }));
        EXPECT_CALL(bus, ReadRegisterMock(0x28, 12)).WillOnce(testing::Return(Frames(2)));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_EQ(6u, received.size());
    }
}
