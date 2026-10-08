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
    class RegisterSensorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectModifyRegister(uint8_t address, uint8_t currentValue, uint8_t resultValue)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ currentValue }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ resultValue }));
        }

        void StartStreaming()
        {
            ExpectModifyRegister(drivers::ImuTestSensor::registerControl, 0x00, drivers::ImuTestSensor::dataReadyBit);

            sensor.AsAccelerometer().Start([this](drivers::ImuTestSensor::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        drivers::ImuTestSensor sensor{ bus, dataReadyPin };
        std::vector<int32_t> received;
    };

    TEST_F(RegisterSensorTest, read_register_passes_the_right_address_and_delivers_the_data)
    {
        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        sensor.TestRead(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0xd4, value);
    }

    TEST_F(RegisterSensorTest, write_register_passes_the_right_address_and_value)
    {
        EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));

        infra::VerifyingFunction<void()> done;
        sensor.TestWrite(0x20, 0x57, done);

        ExecuteAllActions();
    }

    TEST_F(RegisterSensorTest, modify_register_reads_then_writes_with_masked_value)
    {
        // clearMask=0b00001110, setMask=0b00000001, current=0b10101010
        // result = (0b10101010 & ~0b00001110) | 0b00000001 = 0b10100001
        ExpectModifyRegister(0x22, 0b10101010, 0b10100001);

        infra::VerifyingFunction<void()> done;
        sensor.TestModify(0x22, 0b00001110, 0b00000001, done);

        ExecuteAllActions();
    }

    TEST_F(RegisterSensorTest, stop_when_idle_still_reports_done_asynchronously)
    {
        sensor.Initialize();

        bool stopped = false;
        sensor.Stop([&stopped]()
            {
                stopped = true;
            });

        EXPECT_FALSE(stopped);

        ExecuteAllActions();

        EXPECT_TRUE(stopped);
    }

    TEST_F(RegisterSensorTest, stop_defers_until_an_outstanding_transaction_completes)
    {
        sensor.Initialize();
        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x0f, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));

        uint8_t value = 0;
        sensor.TestRead(0x0f, infra::MakeByteRange(value), infra::emptyFunction);

        bool stopped = false;
        sensor.Stop([&stopped]()
            {
                stopped = true;
            });

        EXPECT_FALSE(stopped);

        bus.CompletePending();
        ExecuteAllActions();

        EXPECT_TRUE(stopped);
    }

    TEST_F(RegisterSensorTest, stop_does_not_write_the_device_after_a_modify_read_completes)
    {
        sensor.Initialize();
        bus.completeAutomatically = false;

        EXPECT_CALL(bus, ReadRegisterMock(0x22, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));

        sensor.TestModify(0x22, 0x0e, 0x01, infra::emptyFunction);

        bool stopped = false;
        sensor.Stop([&stopped]()
            {
                stopped = true;
            });

        bus.CompletePending();
        ExecuteAllActions();

        EXPECT_TRUE(stopped);
    }

    TEST_F(RegisterSensorTest, data_ready_pin_edge_triggers_sample_read)
    {
        sensor.Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(drivers::ImuTestSensor::registerOutXLow, 6))
            .WillOnce(testing::Return(std::vector<uint8_t>(6, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();
    }

    TEST_F(RegisterSensorTest, stop_disarms_the_data_ready_interrupt)
    {
        sensor.Initialize();
        StartStreaming();

        infra::VerifyingFunction<void()> stopped;
        sensor.Stop(stopped);
        ExecuteAllActions();

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(received.empty());
    }

    TEST_F(RegisterSensorTest, sensor_without_data_ready_pin_stops_cleanly)
    {
        testing::StrictMock<services::RegisterBusAccessMock> ownBus;
        drivers::ImuTestSensor sensorWithoutPin{ ownBus };
        sensorWithoutPin.Initialize();

        infra::VerifyingFunction<void()> stopped;
        sensorWithoutPin.Stop(stopped);

        ExecuteAllActions();
    }
}
