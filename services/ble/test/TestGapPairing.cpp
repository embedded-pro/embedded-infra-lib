#include "hal/interfaces/MacAddress.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/GapPairing.hpp"
#include "services/ble/test_doubles/GapPairingMock.hpp"
#include "services/ble/test_doubles/GapPairingObserverMock.hpp"
#include "gmock/gmock.h"

namespace services
{
    namespace
    {
        class GapPairingDecoratorTest
            : public testing::Test
        {
        public:
            hal::MacAddress macAddress = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
            std::array<uint8_t, 16> random = { 0x2e, 0x48, 0x9d, 0x25, 0x47, 0x58, 0x53, 0x69, 0x84, 0x29, 0xc9, 0x8b, 0xd3, 0xa6, 0x41, 0xcd };
            std::array<uint8_t, 16> confirm = { 0xf4, 0x2c, 0xcc, 0x10, 0x69, 0x86, 0x95, 0x2e, 0x00, 0x89, 0x4d, 0x50, 0x3f, 0xab, 0x93, 0x39 };

            testing::StrictMock<GapPairingMock> gapPairing;
            GapPairingDecorator decorator{ gapPairing };
            testing::StrictMock<GapPairingObserverMock> gapPairingObserver{ decorator };

            testing::StrictMock<infra::MockCallback<void(GapPairingResult)>> onDoneNotExpected;

            infra::Function<void(GapPairingResult)> RejectedCallback()
            {
                return [this](GapPairingResult result)
                {
                    onDoneNotExpected.callback(result);
                };
            }
        };
    }

    MATCHER_P(OutOfBandDataContentsEqual, x, negation ? "Contents not equal" : "Contents are equal")
    {
        return x.macAddress == arg.macAddress && x.addressType == arg.addressType && infra::ContentsEqual(x.randomData, arg.randomData) && infra::ContentsEqual(x.confirmData, arg.confirmData);
    }

    TEST_F(GapPairingDecoratorTest, forward_all_events_to_observers)
    {
        EXPECT_CALL(gapPairingObserver, DisplayPasskey(::testing::Eq(11111u)));
        EXPECT_CALL(gapPairingObserver, ConfirmNumericComparison(::testing::Eq(222222u)));
        EXPECT_CALL(gapPairingObserver, PairingSuccessfullyCompleted(GapBondStrength{ true, true, 16 }));
        EXPECT_CALL(gapPairingObserver, PairingFailed(::testing::TypedEq<GapPairingResult>(GapPairingResult::numericComparisonFailed)));
        EXPECT_CALL(gapPairingObserver, OutOfBandDataGenerated(OutOfBandDataContentsEqual(GapOutOfBandData{ macAddress, GapDeviceAddressType::publicAddress, infra::MakeByteRange(random), infra::MakeByteRange(confirm) })));

        gapPairing.NotifyObservers([this](GapPairingObserver& obs)
            {
                obs.DisplayPasskey(11111);
                obs.ConfirmNumericComparison(222222);
                obs.PairingSuccessfullyCompleted(GapBondStrength{ true, true, 16 });
                obs.PairingFailed(GapPairingResult::numericComparisonFailed);
                obs.OutOfBandDataGenerated(GapOutOfBandData{ macAddress, GapDeviceAddressType::publicAddress, infra::MakeByteRange(random), infra::MakeByteRange(confirm) });
            });
    }

    TEST_F(GapPairingDecoratorTest, pair_and_bond_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, PairAndBond(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.PairAndBond(infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, pair_and_bond_forwards_failure_result)
    {
        EXPECT_CALL(gapPairing, PairAndBond(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapPairingResult::authenticationRequirementsNotMet), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.PairAndBond(infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::authenticationRequirementsNotMet)));
    }

    TEST_F(GapPairingDecoratorTest, pair_and_bond_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, PairAndBond(testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.PairAndBond(RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, allow_pairing_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, AllowPairing(::testing::IsTrue(), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.AllowPairing(true, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, allow_pairing_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, AllowPairing(::testing::IsFalse(), testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.AllowPairing(false, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, displays_a_passkey_without_a_comparison_flag)
    {
        // Passkey Entry and Numeric Comparison are distinct Security Manager procedures.
        EXPECT_CALL(gapPairingObserver, DisplayPasskey(123456u));

        gapPairing.NotifyObservers([](GapPairingObserver& obs)
            {
                obs.DisplayPasskey(123456);
            });
    }

    TEST_F(GapPairingDecoratorTest, confirms_a_numeric_comparison_through_its_own_callback)
    {
        EXPECT_CALL(gapPairingObserver, ConfirmNumericComparison(654321u));

        gapPairing.NotifyObservers([](GapPairingObserver& obs)
            {
                obs.ConfirmNumericComparison(654321);
            });
    }

    TEST_F(GapPairingDecoratorTest, carries_the_whole_six_digit_passkey_range)
    {
        // A passkey is six digits, 000000 to 999999.
        EXPECT_CALL(gapPairingObserver, DisplayPasskey(0u));
        EXPECT_CALL(gapPairingObserver, DisplayPasskey(999999u));

        gapPairing.NotifyObservers([](GapPairingObserver& obs)
            {
                obs.DisplayPasskey(0);
                obs.DisplayPasskey(999999);
            });
    }

    TEST_F(GapPairingDecoratorTest, reports_the_bond_strength_when_pairing_completes)
    {
        const GapBondStrength lesc{ true, true, 16 };

        EXPECT_CALL(gapPairingObserver, PairingSuccessfullyCompleted(lesc));

        gapPairing.NotifyObservers([&lesc](GapPairingObserver& obs)
            {
                obs.PairingSuccessfullyCompleted(lesc);
            });
    }

    TEST_F(GapPairingDecoratorTest, distinguishes_a_just_works_bond_from_an_authenticated_one)
    {
        const GapBondStrength justWorks{ true, false, 16 };

        EXPECT_CALL(gapPairingObserver, PairingSuccessfullyCompleted(justWorks));

        gapPairing.NotifyObservers([&justWorks](GapPairingObserver& obs)
            {
                obs.PairingSuccessfullyCompleted(justWorks);
            });

        EXPECT_NE((GapBondStrength{ true, true, 16 }), justWorks);
        EXPECT_NE((GapBondStrength{ false, false, 16 }), justWorks);
        EXPECT_NE((GapBondStrength{ true, false, 7 }), justWorks);
    }

    TEST_F(GapPairingDecoratorTest, set_security_mode_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, SetSecurityMode(GapPairing::SecurityModeAndLevel::mode1Level1, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetSecurityMode(GapPairing::SecurityModeAndLevel::mode1Level1, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, set_security_mode_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, SetSecurityMode(GapPairing::SecurityModeAndLevel::mode1Level4, testing::_))
            .WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.SetSecurityMode(GapPairing::SecurityModeAndLevel::mode1Level4, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, offers_every_mode_and_level_the_specification_defines_and_no_others)
    {
        // Mode 2 stops at level 2.
        for (auto modeAndLevel : { GapPairing::SecurityModeAndLevel::mode1Level1, GapPairing::SecurityModeAndLevel::mode1Level2,
                 GapPairing::SecurityModeAndLevel::mode1Level3, GapPairing::SecurityModeAndLevel::mode1Level4,
                 GapPairing::SecurityModeAndLevel::mode2Level1, GapPairing::SecurityModeAndLevel::mode2Level2 })
        {
            EXPECT_CALL(gapPairing, SetSecurityMode(modeAndLevel, testing::_))
                .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

            EXPECT_EQ(GapRequestStatus::accepted, decorator.SetSecurityMode(modeAndLevel, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
        }
    }

    TEST_F(GapPairingDecoratorTest, set_secure_connections_only_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, SetSecureConnectionsOnly(true, testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetSecureConnectionsOnly(true, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, set_secure_connections_only_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, SetSecureConnectionsOnly(false, testing::_))
            .WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.SetSecureConnectionsOnly(false, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, set_io_capabilities_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, SetIoCapabilities(::testing::TypedEq<GapPairing::IoCapabilities>(GapPairing::IoCapabilities::none), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetIoCapabilities(GapPairing::IoCapabilities::none, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, set_io_capabilities_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, SetIoCapabilities(::testing::TypedEq<GapPairing::IoCapabilities>(GapPairing::IoCapabilities::keyboardDisplay), testing::_))
            .WillOnce(testing::Return(GapRequestStatus::busy));

        EXPECT_EQ(GapRequestStatus::busy, decorator.SetIoCapabilities(GapPairing::IoCapabilities::keyboardDisplay, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, generate_out_of_band_data_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, GenerateOutOfBandData(testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<0>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.GenerateOutOfBandData(infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, generate_out_of_band_data_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, GenerateOutOfBandData(testing::_)).WillOnce(testing::Return(GapRequestStatus::notSupported));

        EXPECT_EQ(GapRequestStatus::notSupported, decorator.GenerateOutOfBandData(RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, set_out_of_band_data_forwards_request_and_result)
    {
        GapOutOfBandData outOfBandData{ hal::MacAddress(), GapDeviceAddressType::publicAddress, infra::MakeByteRange(random), infra::MakeByteRange(confirm) };

        EXPECT_CALL(gapPairing, SetOutOfBandData(OutOfBandDataContentsEqual(outOfBandData), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.SetOutOfBandData(outOfBandData, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, set_out_of_band_data_forwards_rejection_without_invoking_callback)
    {
        GapOutOfBandData outOfBandData{ hal::MacAddress(), GapDeviceAddressType::publicAddress, infra::MakeByteRange(random), infra::MakeByteRange(confirm) };

        EXPECT_CALL(gapPairing, SetOutOfBandData(OutOfBandDataContentsEqual(outOfBandData), testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidParameter));

        EXPECT_EQ(GapRequestStatus::invalidParameter, decorator.SetOutOfBandData(outOfBandData, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, authenticate_with_passkey_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, AuthenticateWithPasskey(::testing::Eq(11111), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::success), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.AuthenticateWithPasskey(11111, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::success)));
    }

    TEST_F(GapPairingDecoratorTest, authenticate_with_passkey_forwards_failure_result)
    {
        EXPECT_CALL(gapPairing, AuthenticateWithPasskey(::testing::Eq(22222), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::passkeyEntryFailed), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.AuthenticateWithPasskey(22222, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::passkeyEntryFailed)));
    }

    TEST_F(GapPairingDecoratorTest, authenticate_with_passkey_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, AuthenticateWithPasskey(::testing::Eq(11111), testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.AuthenticateWithPasskey(11111, RejectedCallback()));
    }

    TEST_F(GapPairingDecoratorTest, numeric_comparison_confirm_forwards_request_and_result)
    {
        EXPECT_CALL(gapPairing, NumericComparisonConfirm(::testing::IsFalse(), testing::_))
            .WillOnce(testing::DoAll(testing::InvokeArgument<1>(GapPairingResult::numericComparisonFailed), testing::Return(GapRequestStatus::accepted)));

        EXPECT_EQ(GapRequestStatus::accepted, decorator.NumericComparisonConfirm(false, infra::VerifyingFunction<void(GapPairingResult)>(GapPairingResult::numericComparisonFailed)));
    }

    TEST_F(GapPairingDecoratorTest, numeric_comparison_confirm_forwards_rejection_without_invoking_callback)
    {
        EXPECT_CALL(gapPairing, NumericComparisonConfirm(::testing::IsTrue(), testing::_)).WillOnce(testing::Return(GapRequestStatus::invalidState));

        EXPECT_EQ(GapRequestStatus::invalidState, decorator.NumericComparisonConfirm(true, RejectedCallback()));
    }
}
