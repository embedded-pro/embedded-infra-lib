#include "drivers/imu/common/SensorWithPolling.hpp"
#include "drivers/imu/common/test/ImuTestSensor.hpp"
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
    using PolledSensor = drivers::SensorWithPolling<drivers::ImuTestSensor>;

    class SensorWithPollingTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming()
        {
            ExpectModifyRegister(drivers::ImuTestSensor::registerControl, 0x00, drivers::ImuTestSensor::dataReadyBit);

            device.AsAccelerometer().Start([this](drivers::ImuTestSensor::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        PolledSensor device{ bus };
        std::vector<int32_t> received;
    };

    TEST_F(SensorWithPollingTest, poll_tick_reads_status_and_delivers_when_data_available_bit_is_set)
    {
        device.Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerStatus, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ drivers::ImuTestSensor::dataAvailable }));
        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerOutXLow, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        ForwardTime(std::chrono::milliseconds(5));
    }

    TEST_F(SensorWithPollingTest, poll_tick_reads_status_but_does_not_deliver_when_data_bit_is_clear)
    {
        device.Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerStatus, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_TRUE(received.empty());
    }

    TEST_F(SensorWithPollingTest, verify_data_ready_disabled_skips_the_status_read)
    {
        device.Initialize();
        device.SetVerifyDataReady(false);
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerOutXLow, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        ForwardTime(std::chrono::milliseconds(5));
    }

    TEST_F(SensorWithPollingTest, a_tick_is_skipped_while_a_bus_transaction_is_outstanding)
    {
        device.Initialize();
        StartStreaming();

        bus.completeAutomatically = false;

        // Return no-data so the status callback does not chain into a sample read
        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerStatus, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(5));

        // Transaction is still outstanding; a second tick must not issue another read
        ForwardTime(std::chrono::milliseconds(5));

        bus.CompletePending();
        ExecuteAllActions();
    }

    TEST_F(SensorWithPollingTest, a_tick_is_skipped_while_a_read_modify_write_is_in_flight)
    {
        device.Initialize();
        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerControl, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        device.AsAccelerometer().Start([](drivers::ImuTestSensor::Accelerometer::Samples) {});

        // The tick lands while the read half of the data ready read-modify-write is still on the
        // bus; a status read now would be a second concurrent transaction
        ForwardTime(std::chrono::milliseconds(5));

        EXPECT_CALL(bus, WriteRegisterMock(drivers::ImuTestSensor::registerControl, std::vector<uint8_t>{ drivers::ImuTestSensor::dataReadyBit }));

        bus.CompletePending();
        bus.CompletePending();
        ExecuteAllActions();
    }

    TEST_F(SensorWithPollingTest, stop_sampling_cancels_the_poll_timer)
    {
        device.Initialize();
        StartStreaming();

        ExpectModifyRegister(drivers::ImuTestSensor::registerControl, drivers::ImuTestSensor::dataReadyBit, 0x00);

        device.AsAccelerometer().Stop();
        ExecuteAllActions();

        ForwardTime(std::chrono::milliseconds(50));

        EXPECT_TRUE(received.empty());
    }

    TEST_F(SensorWithPollingTest, polling_interval_is_configurable)
    {
        device.Initialize();
        device.SetPollingInterval(std::chrono::milliseconds(20));
        StartStreaming();

        ForwardTime(std::chrono::milliseconds(19));

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerStatus, 1))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        ForwardTime(std::chrono::milliseconds(1));
    }
}
