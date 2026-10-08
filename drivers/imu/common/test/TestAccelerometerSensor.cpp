#include "drivers/imu/common/AccelerometerSensor.hpp"
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
    class AccelerometerSensorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        using Device = drivers::ImuTestSensor;
        using Acceleration = Device::Acceleration;

        void ExpectModifyRegister(uint8_t address, uint8_t current, uint8_t result)
        {
            EXPECT_CALL(bus, ReadRegisterMock(address, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ current }));
            EXPECT_CALL(bus, WriteRegisterMock(address, std::vector<uint8_t>{ result }));
        }

        void StartStreaming()
        {
            ExpectModifyRegister(Device::registerControl, 0x00, Device::dataReadyBit);

            device.AsAccelerometer().Start([this](Device::Accelerometer::Samples samples)
                {
                    for (auto sample : samples)
                        received.push_back(sample.Value());
                });

            ExecuteAllActions();
        }

        static std::vector<uint8_t> Measurement(int16_t x, int16_t y, int16_t z)
        {
            std::vector<uint8_t> data;
            for (int16_t count : { x, y, z })
            {
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(count) & 0xff));
                data.push_back(static_cast<uint8_t>(static_cast<uint16_t>(count) >> 8));
            }
            return data;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub dataReadyPin;
        Device device{ bus, dataReadyPin };
        std::vector<int32_t> received;
    };

    TEST_F(AccelerometerSensorTest, start_and_stop_lifecycle)
    {
        device.Initialize();
        StartStreaming();

        ExpectModifyRegister(Device::registerControl, Device::dataReadyBit, 0x00);

        device.AsAccelerometer().Stop();
        ExecuteAllActions();
    }

    TEST_F(AccelerometerSensorTest, data_ready_delivers_three_samples_x_y_z)
    {
        device.Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(Device::registerOutXLow, 6))
            .WillOnce(testing::Return(Measurement(1000, 500, -200)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_EQ(3u, received.size());
        EXPECT_EQ(598, received[0]);
        EXPECT_EQ(299, received[1]);
        EXPECT_EQ(-120, received[2]);
    }

    TEST_F(AccelerometerSensorTest, negative_samples_produce_negative_acceleration)
    {
        device.Initialize();
        StartStreaming();

        EXPECT_CALL(bus, ReadRegisterMock(Device::registerOutXLow, 6))
            .WillOnce(testing::Return(Measurement(-1000, 0, 0)));

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        ASSERT_EQ(3u, received.size());
        EXPECT_EQ(-598, received[0]);
    }

    TEST_F(AccelerometerSensorTest, one_thousand_counts_at_61_ug_per_count_gives_598_mm_per_second_squared)
    {
        auto result = Device::TestToAcceleration(1000, 61);
        EXPECT_EQ(598, result.Value());
    }

    TEST_F(AccelerometerSensorTest, rounding_is_half_away_from_zero_for_positive_values)
    {
        auto result = Device::TestToAcceleration(1, 61);
        EXPECT_EQ(1, result.Value());
    }

    TEST_F(AccelerometerSensorTest, rounding_is_half_away_from_zero_for_negative_values)
    {
        auto result = Device::TestToAcceleration(-1, 61);
        EXPECT_EQ(-1, result.Value());
    }

    TEST_F(AccelerometerSensorTest, positive_32768_at_1952_ug_does_not_overflow)
    {
        auto result = Device::TestToAcceleration(32768, 1952);
        EXPECT_GT(result.Value(), 0);
    }

    TEST_F(AccelerometerSensorTest, negative_32768_at_1952_ug_does_not_overflow)
    {
        auto result = Device::TestToAcceleration(-32768, 1952);
        EXPECT_LT(result.Value(), 0);
    }

    TEST_F(AccelerometerSensorTest, extreme_counts_at_732_ug_do_not_overflow)
    {
        auto positive = Device::TestToAcceleration(32768, 732);
        auto negative = Device::TestToAcceleration(-32768, 732);
        EXPECT_GT(positive.Value(), 0);
        EXPECT_LT(negative.Value(), 0);
    }

    TEST_F(AccelerometerSensorTest, extreme_counts_at_72000_ug_do_not_overflow)
    {
        auto positive = Device::TestToAcceleration(128, 72000);
        auto negative = Device::TestToAcceleration(-128, 72000);
        EXPECT_GT(positive.Value(), 0);
        EXPECT_LT(negative.Value(), 0);
    }

    TEST_F(AccelerometerSensorTest, samples_not_delivered_after_stop)
    {
        device.Initialize();
        StartStreaming();

        infra::VerifyingFunction<void()> stopped;
        device.Stop(stopped);
        ExecuteAllActions();

        dataReadyPin.SetStubState(true);
        ExecuteAllActions();

        EXPECT_TRUE(received.empty());
    }
}
