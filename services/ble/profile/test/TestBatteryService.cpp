#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "services/ble/profile/BatteryService.hpp"
#include "services/ble/test_doubles/GattServerMock.hpp"
#include "gmock/gmock.h"
#include <optional>

namespace
{
    class BatteryServiceTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        BatteryServiceTest()
        {
            EXPECT_CALL(gattServer, AddService(testing::_));
            batteryService.emplace(gattServer);

            for (auto& characteristic : batteryService->Service().Characteristics())
                characteristic.Attach(operations);
        }

        services::GattServerCharacteristic& BatteryLevelCharacteristic()
        {
            return batteryService->Service().Characteristics().front();
        }

        testing::StrictMock<services::GattServerMock> gattServer;
        testing::StrictMock<services::GattServerCharacteristicOperationsMock> operations;
        std::optional<services::BatteryService> batteryService;
    };
}

TEST_F(BatteryServiceTest, exposes_a_readable_and_notifiable_battery_level)
{
    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::batteryService }, batteryService->Service().Type());
    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::batteryLevel }, BatteryLevelCharacteristic().Type());
    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::read | services::GattCharacteristic::PropertyFlags::notify, BatteryLevelCharacteristic().Properties());
    EXPECT_EQ(1, BatteryLevelCharacteristic().ValueLength());
}

TEST_F(BatteryServiceTest, starts_at_an_empty_battery)
{
    EXPECT_EQ(0, batteryService->BatteryLevel());
}

TEST_F(BatteryServiceTest, a_new_level_is_updated_as_a_single_octet)
{
    std::array<uint8_t, 1> expected{ 42 };

    EXPECT_CALL(operations, Update(testing::Ref(BatteryLevelCharacteristic()), testing::ElementsAreArray(expected))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    batteryService->BatteryLevelChanged(42);

    EXPECT_EQ(42, batteryService->BatteryLevel());
}

TEST_F(BatteryServiceTest, a_full_battery_is_accepted)
{
    std::array<uint8_t, 1> expected{ services::BatteryService::maxBatteryLevel };

    EXPECT_CALL(operations, Update(testing::_, testing::ElementsAreArray(expected))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    batteryService->BatteryLevelChanged(services::BatteryService::maxBatteryLevel);
}

TEST_F(BatteryServiceTest, a_busy_stack_is_retried)
{
    EXPECT_CALL(operations, Update(testing::_, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::busy)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    batteryService->BatteryLevelChanged(7);

    ExecuteAllActions();
}
