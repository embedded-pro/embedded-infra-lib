#include "services/ble/PersistentBondStorage.hpp"
#include "services/util/test_doubles/ConfigurationStoreMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>
#include <vector>

namespace
{
    class PersistentBondStorageTest
        : public testing::Test
    {
    public:
        void Construct()
        {
            storage.emplace(services::ConfigurationStoreAccess<infra::BoundedVector<uint8_t>>{ store, stored });
        }

        std::vector<hal::MacAddress> Bonded()
        {
            std::vector<hal::MacAddress> result;
            storage->IterateBondedDevices([&result](hal::MacAddress address)
                {
                    result.push_back(address);
                });
            return result;
        }

        std::vector<uint8_t> Stored() const
        {
            return std::vector<uint8_t>(stored.begin(), stored.end());
        }

        const hal::MacAddress first{ 1, 2, 3, 4, 5, 6 };
        const hal::MacAddress second{ 2, 3, 4, 5, 6, 7 };
        const hal::MacAddress third{ 3, 4, 5, 6, 7, 8 };
        testing::StrictMock<services::ConfigurationStoreInterfaceMock> store;
        infra::BoundedVector<uint8_t>::WithMaxSize<12> stored;
        std::optional<services::PersistentBondStorage> storage;
    };
}

TEST_F(PersistentBondStorageTest, restores_the_stored_bonds_without_writing)
{
    stored.insert(stored.end(), first.begin(), first.end());
    stored.insert(stored.end(), second.begin(), second.end());

    Construct();

    EXPECT_EQ((std::vector<hal::MacAddress>{ first, second }), Bonded());
}

TEST_F(PersistentBondStorageTest, starts_empty_from_an_empty_store)
{
    Construct();

    EXPECT_TRUE(Bonded().empty());
    EXPECT_FALSE(storage->IsBondStored(first));
}

TEST_F(PersistentBondStorageTest, holds_as_many_bonds_as_the_store_has_room_for)
{
    Construct();

    EXPECT_EQ(2u, storage->GetMaxNumberOfBonds());
}

TEST_F(PersistentBondStorageTest, ignores_an_incomplete_trailing_address)
{
    stored.insert(stored.end(), first.begin(), first.end());
    stored.push_back(9);

    Construct();

    EXPECT_EQ(std::vector<hal::MacAddress>{ first }, Bonded());
}

TEST_F(PersistentBondStorageTest, an_already_bonded_device_is_not_written_again)
{
    Construct();
    EXPECT_CALL(store, Write()).WillOnce(testing::Return(1));

    storage->UpdateBondedDevice(first);
    storage->UpdateBondedDevice(first);

    EXPECT_TRUE(storage->IsBondStored(first));
}

TEST_F(PersistentBondStorageTest, a_new_bond_is_written)
{
    Construct();
    EXPECT_CALL(store, Write()).WillOnce(testing::Return(1));

    storage->UpdateBondedDevice(first);

    EXPECT_EQ((std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }), Stored());
}

TEST_F(PersistentBondStorageTest, evicting_the_oldest_bond_is_written)
{
    Construct();
    EXPECT_CALL(store, Write()).Times(3).WillRepeatedly(testing::Return(1));

    storage->UpdateBondedDevice(first);
    storage->UpdateBondedDevice(second);
    storage->UpdateBondedDevice(third);

    EXPECT_EQ((std::vector<uint8_t>{ 2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7, 8 }), Stored());
}

TEST_F(PersistentBondStorageTest, removals_are_written)
{
    Construct();
    EXPECT_CALL(store, Write()).Times(4).WillRepeatedly(testing::Return(1));
    storage->UpdateBondedDevice(first);
    storage->UpdateBondedDevice(second);

    storage->RemoveBond(first);
    EXPECT_EQ((std::vector<uint8_t>{ 2, 3, 4, 5, 6, 7 }), Stored());

    storage->RemoveAllBonds();
    EXPECT_TRUE(Stored().empty());
}

TEST_F(PersistentBondStorageTest, conditional_removal_is_written)
{
    Construct();
    EXPECT_CALL(store, Write()).Times(3).WillRepeatedly(testing::Return(1));
    storage->UpdateBondedDevice(first);
    storage->UpdateBondedDevice(second);

    storage->RemoveBondIf([this](hal::MacAddress address)
        {
            return address == second;
        });

    EXPECT_EQ((std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }), Stored());
}
