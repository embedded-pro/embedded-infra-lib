#include "services/ble/VolatileBondStorage.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

namespace
{
    class VolatileBondStorageTest
        : public testing::Test
    {
    public:
        std::vector<hal::MacAddress> Stored()
        {
            std::vector<hal::MacAddress> result;
            storage.IterateBondedDevices([&result](hal::MacAddress address)
                {
                    result.push_back(address);
                });
            return result;
        }

        const hal::MacAddress first{ 1, 2, 3, 4, 5, 6 };
        const hal::MacAddress second{ 2, 3, 4, 5, 6, 7 };
        const hal::MacAddress third{ 3, 4, 5, 6, 7, 8 };
        services::VolatileBondStorage::WithMaxBonds<2> storage;
    };
}

TEST_F(VolatileBondStorageTest, starts_empty_with_its_capacity_as_maximum)
{
    EXPECT_TRUE(Stored().empty());
    EXPECT_EQ(2, storage.GetMaxNumberOfBonds());
}

TEST_F(VolatileBondStorageTest, stores_a_bonded_device_once)
{
    storage.UpdateBondedDevice(first);
    storage.UpdateBondedDevice(first);

    EXPECT_TRUE(storage.IsBondStored(first));
    EXPECT_FALSE(storage.IsBondStored(second));
    EXPECT_EQ(std::vector<hal::MacAddress>{ first }, Stored());
}

TEST_F(VolatileBondStorageTest, evicts_the_oldest_bond_when_full)
{
    storage.UpdateBondedDevice(first);
    storage.UpdateBondedDevice(second);
    storage.UpdateBondedDevice(third);

    EXPECT_EQ((std::vector<hal::MacAddress>{ second, third }), Stored());
}

TEST_F(VolatileBondStorageTest, removes_a_single_bond)
{
    storage.UpdateBondedDevice(first);
    storage.UpdateBondedDevice(second);

    storage.RemoveBond(first);

    EXPECT_EQ(std::vector<hal::MacAddress>{ second }, Stored());
}

TEST_F(VolatileBondStorageTest, removes_bonds_matching_a_predicate)
{
    storage.UpdateBondedDevice(first);
    storage.UpdateBondedDevice(second);

    storage.RemoveBondIf([this](hal::MacAddress address)
        {
            return address == second;
        });

    EXPECT_EQ(std::vector<hal::MacAddress>{ first }, Stored());
}

TEST_F(VolatileBondStorageTest, removes_all_bonds)
{
    storage.UpdateBondedDevice(first);
    storage.UpdateBondedDevice(second);

    storage.RemoveAllBonds();

    EXPECT_TRUE(Stored().empty());
}
