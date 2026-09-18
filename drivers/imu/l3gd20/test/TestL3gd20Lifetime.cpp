#include "drivers/imu/l3gd20/L3gd20WithFifo.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Device = drivers::L3gd20Core;

    class L3gd20LifetimeTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static Device::Config DefaultConfig()
        {
            Device::Config config;
            config.turnOnTime = std::chrono::milliseconds(5);

            return config;
        }

        static void ExpectInitializationOn(services::RegisterBusAccessMock& target)
        {
            EXPECT_CALL(target, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));
            EXPECT_CALL(target, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(target, WriteRegisterMock(0x21, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x23, std::vector<uint8_t>{ 0x80 }));
            EXPECT_CALL(target, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x25, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x2e, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(target, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));
        }

        void Initialize()
        {
            ExpectInitializationOn(bus);

            device.Initialize(DefaultConfig(), [](Device::InitializationResult) {});
            ForwardTime(std::chrono::milliseconds(30));
        }

        void StartStreaming()
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(bus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));

            device.AsGyroscope().Start([this](Device::Gyroscope::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        hal::GpioPinStub interruptPin;
        Device device{ bus, dataReadyPin, interruptPin };
        std::vector<int32_t> received;
    };

    TEST_F(L3gd20LifetimeTest, stop_when_idle_still_reports_done)
    {
        Initialize();

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);

        ExecuteAllActions();
    }

    TEST_F(L3gd20LifetimeTest, stop_disarms_the_data_ready_line_and_delivers_no_further_samples)
    {
        Initialize();
        StartStreaming();

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);
        ExecuteAllActions();

        // A strict mock fails the test if the edge still reached the bus
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(received.empty());
    }

    TEST_F(L3gd20LifetimeTest, stop_defers_until_an_outstanding_transaction_completes)
    {
        Initialize();
        StartStreaming();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x28, 6)).WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));
        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(bus.CompletionPending());

        bool stopped = false;
        device.Stop([&stopped]()
            {
                stopped = true;
            });

        ExecuteAllActions();
        EXPECT_FALSE(stopped);

        bus.CompletePending();
        ExecuteAllActions();

        EXPECT_TRUE(stopped);
    }

    // The read of a read-modify-write may land after Stop; the device must not be written afterwards
    TEST_F(L3gd20LifetimeTest, stop_does_not_write_the_device_after_a_modify_read_completes)
    {
        Initialize();

        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        device.AsGyroscope().Start([](Device::Gyroscope::Samples) {});
        ExecuteAllActions();

        EXPECT_TRUE(bus.CompletionPending());

        bool stopped = false;
        device.Stop([&stopped]()
            {
                stopped = true;
            });

        bus.CompletePending();
        ExecuteAllActions();

        EXPECT_TRUE(stopped);
    }

    TEST_F(L3gd20LifetimeTest, stop_aborts_a_running_sequence_without_invoking_its_callback)
    {
        // Only the steps ahead of the reboot delay run; a strict mock fails the test if the
        // configuration writes behind it are issued anyway
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x00 }));
        EXPECT_CALL(bus, WriteRegisterMock(0x24, std::vector<uint8_t>{ 0x80 }));

        bool initialized = false;
        device.Initialize(DefaultConfig(), [&initialized](Device::InitializationResult)
            {
                initialized = true;
            });

        // Part way through the sequence, while the reboot delay is still running
        ForwardTime(std::chrono::milliseconds(10));
        EXPECT_FALSE(initialized);

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);

        ForwardTime(std::chrono::milliseconds(30));

        EXPECT_FALSE(initialized);
        EXPECT_FALSE(device.Initialized());
    }

    TEST_F(L3gd20LifetimeTest, a_device_can_be_destroyed_once_stop_has_reported_done)
    {
        testing::StrictMock<services::RegisterBusAccessMock> ownBus;
        hal::GpioPinStub ownPin;

        {
            Device owned{ ownBus, ownPin };

            ExpectInitializationOn(ownBus);
            owned.Initialize(DefaultConfig(), [](Device::InitializationResult) {});
            ForwardTime(std::chrono::milliseconds(30));

            EXPECT_CALL(ownBus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
            EXPECT_CALL(ownBus, WriteRegisterMock(0x22, std::vector<uint8_t>{ 0x08 }));

            owned.AsGyroscope().Start([](Device::Gyroscope::Samples) {});
            ExecuteAllActions();

            infra::VerifyingFunction<void()> stopped;
            owned.Stop(stopped);
            ExecuteAllActions();
        }

        // A further edge on the pin the destroyed device held must reach nothing
        ownPin.SetStubState(true);
        ExecuteAllActions();
    }
}
