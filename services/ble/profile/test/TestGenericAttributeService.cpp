#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "services/ble/profile/GenericAttributeService.hpp"
#include "services/ble/test_doubles/GattServerMock.hpp"
#include "gmock/gmock.h"
#include <optional>

namespace
{
    class GenericAttributeServiceTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        GenericAttributeServiceTest()
        {
            EXPECT_CALL(gattServer, AddService(testing::_));
            genericAttributeService.emplace(gattServer);

            for (auto& characteristic : genericAttributeService->Service().Characteristics())
                characteristic.Attach(operations);
        }

        services::GattServerCharacteristic& ServiceChangedCharacteristic()
        {
            return genericAttributeService->Service().Characteristics().front();
        }

        testing::StrictMock<services::GattServerMock> gattServer;
        testing::StrictMock<services::GattServerCharacteristicOperationsMock> operations;
        std::optional<services::GenericAttributeService> genericAttributeService;
    };
}

TEST_F(GenericAttributeServiceTest, exposes_an_indicatable_service_changed)
{
    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::genericAttributeService }, genericAttributeService->Service().Type());
    EXPECT_EQ(services::AttAttribute::Uuid{ services::uuid::serviceChanged }, ServiceChangedCharacteristic().Type());
    EXPECT_EQ(services::GattCharacteristic::PropertyFlags::indicate, ServiceChangedCharacteristic().Properties());
    EXPECT_EQ(4, ServiceChangedCharacteristic().ValueLength());
}

TEST_F(GenericAttributeServiceTest, a_changed_range_is_indicated_as_the_value_a_client_decodes)
{
    std::optional<services::GattServiceChanged> indicated;

    EXPECT_CALL(operations, Update(testing::Ref(ServiceChangedCharacteristic()), testing::_)).WillOnce([&indicated](auto&, infra::ConstByteRange data)
        {
            indicated = services::GattServiceChangedFromValue(data);
            return services::GattRequestStatus::accepted;
        });

    genericAttributeService->ServiceChanged(0x0010, 0x001F);

    ASSERT_TRUE(indicated);
    EXPECT_EQ((services::GattServiceChanged{ 0x0010, 0x001F }), *indicated);
}

TEST_F(GenericAttributeServiceTest, a_changed_range_travels_little_endian)
{
    std::array<uint8_t, 4> expected{ 0x34, 0x12, 0x78, 0x56 };

    EXPECT_CALL(operations, Update(testing::_, testing::ElementsAreArray(expected))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    genericAttributeService->ServiceChanged(services::GattServiceChanged{ 0x1234, 0x5678 });
}

TEST_F(GenericAttributeServiceTest, a_single_changed_handle_is_a_range_of_one)
{
    std::array<uint8_t, 4> expected{ 0x21, 0x00, 0x21, 0x00 };

    EXPECT_CALL(operations, Update(testing::_, testing::ElementsAreArray(expected))).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    genericAttributeService->ServiceChanged(0x0021, 0x0021);
}
