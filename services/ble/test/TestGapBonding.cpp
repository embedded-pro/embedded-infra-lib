#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/GapBonding.hpp"
#include "services/ble/test_doubles/GapBondingMock.hpp"
#include "services/ble/test_doubles/GapBondingObserverMock.hpp"
#include "gmock/gmock.h"

namespace services
{
    namespace
    {
        class GapBondingDecoratorTest
            : public testing::Test
        {
        public:
            testing::StrictMock<GapBondingMock> gapBonding;
            GapBondingDecorator decorator{ gapBonding };
            testing::StrictMock<GapBondingObserverMock> gapBondingObserver{ decorator };

            testing::StrictMock<infra::MockCallback<void()>> onDoneNotExpected;

            infra::Function<void()> RejectedCallback()
            {
                return [this]()
                {
                    onDoneNotExpected.callback();
                };
            }
        };
    }

    TEST_F(GapBondingDecoratorTest, forward_all_events_to_observers)
    {
        EXPECT_CALL(gapBondingObserver, NumberOfBondsChanged(::testing::Eq(10)));

        gapBonding.NotifyObservers([](GapBondingObserver& obs)
            {
                obs.NumberOfBondsChanged(10);
            });
    }

    TEST_F(GapBondingDecoratorTest, forward_all_getters_to_subject)
    {
        EXPECT_CALL(gapBonding, GetMaxNumberOfBonds()).WillOnce(testing::Return(5));
        EXPECT_EQ(decorator.GetMaxNumberOfBonds(), 5);

        EXPECT_CALL(gapBonding, GetNumberOfBonds()).WillOnce(testing::Return(5));
        EXPECT_EQ(decorator.GetNumberOfBonds(), 5);

        hal::MacAddress mac = { 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };
        GapDeviceAddressType addressType = GapDeviceAddressType::randomAddress;

        EXPECT_CALL(gapBonding, IsDeviceBonded(GapAddress{ mac, addressType })).WillOnce(testing::Return(true));
        EXPECT_THAT(decorator.IsDeviceBonded(GapAddress{ mac, addressType }), testing::IsTrue());

        EXPECT_CALL(gapBonding, IsDeviceBonded(GapAddress{ mac, addressType })).WillOnce(testing::Return(false));
        EXPECT_THAT(decorator.IsDeviceBonded(GapAddress{ mac, addressType }), testing::IsFalse());
    }

    TEST_F(GapBondingDecoratorTest, remove_all_bonds_forwards_request_and_completion)
    {
        EXPECT_CALL(gapBonding, RemoveAllBonds(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.RemoveAllBonds(infra::VerifyingFunction<void()>()));
    }

    TEST_F(GapBondingDecoratorTest, remove_all_bonds_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapBonding, RemoveAllBonds(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.RemoveAllBonds(RejectedCallback()));
    }

    TEST_F(GapBondingDecoratorTest, remove_oldest_bond_forwards_request_and_completion)
    {
        EXPECT_CALL(gapBonding, RemoveOldestBond(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.RemoveOldestBond(infra::VerifyingFunction<void()>()));
    }

    TEST_F(GapBondingDecoratorTest, reports_the_strength_of_a_bonded_device)
    {
        const hal::MacAddress mac = { 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };
        const GapBondStrength lesc{ true, true, 16 };

        EXPECT_CALL(gapBonding, BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress })).WillOnce(testing::Return(lesc));

        auto strength = decorator.BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress });

        ASSERT_TRUE(strength);
        EXPECT_TRUE(strength->secureConnections);
        EXPECT_TRUE(strength->authenticated);
        EXPECT_EQ(16, strength->encryptionKeySize);
    }

    TEST_F(GapBondingDecoratorTest, reports_no_strength_for_a_device_that_is_not_bonded)
    {
        const hal::MacAddress mac = { 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };

        EXPECT_CALL(gapBonding, BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress })).WillOnce(testing::Return(std::nullopt));

        EXPECT_FALSE(decorator.BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress }));
    }

    TEST_F(GapBondingDecoratorTest, distinguishes_a_legacy_bond_from_a_secure_connections_one)
    {
        const hal::MacAddress mac = { 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };

        // The distinction an application needs before trusting a bonded peer with a privileged
        // operation, and the one IsDeviceBonded cannot express.
        const GapBondStrength legacyUnauthenticated{ false, false, 16 };

        EXPECT_CALL(gapBonding, BondStrength(GapAddress{ mac, GapDeviceAddressType::randomAddress })).WillOnce(testing::Return(legacyUnauthenticated));

        auto strength = decorator.BondStrength(GapAddress{ mac, GapDeviceAddressType::randomAddress });

        ASSERT_TRUE(strength);
        EXPECT_FALSE(strength->secureConnections);
        EXPECT_FALSE(strength->authenticated);
    }

    TEST_F(GapBondingDecoratorTest, reports_a_short_encryption_key)
    {
        const hal::MacAddress mac = { 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };

        // A 7-octet key is the specification's minimum and is worth far less than a 16-octet one,
        // which a bonded-or-not answer cannot convey.
        const GapBondStrength shortKey{ true, true, 7 };

        EXPECT_CALL(gapBonding, BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress })).WillOnce(testing::Return(shortKey));

        EXPECT_EQ(7, decorator.BondStrength(GapAddress{ mac, GapDeviceAddressType::publicAddress })->encryptionKeySize);
    }

    TEST_F(GapBondingDecoratorTest, remove_oldest_bond_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapBonding, RemoveOldestBond(testing::_)).WillOnce(testing::Return(GapRequestStatus::busy));

        EXPECT_EQ(GapRequestStatus::busy, decorator.RemoveOldestBond(RejectedCallback()));
    }
}
