#include "generated/echo/GapCentral.pb.hpp"
#include "generated/echo/GapPeripheral.pb.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/stream/ByteOutputStream.hpp"
#include "infra/syntax/ProtoFormatter.hpp"
#include "infra/syntax/ProtoParser.hpp"
#include "infra/util/EnumCast.hpp"
#include "services/ble/GapPairing.hpp"
#include "gmock/gmock.h"

namespace
{
    template<class Message>
    Message RoundTrip(const Message& message)
    {
        infra::ByteOutputStream::WithStorage<128> stream;
        infra::ProtoFormatter formatter(stream);
        message.Serialize(formatter);

        infra::ByteInputStream inputStream(stream.Writer().Processed());
        infra::ProtoParser parser(inputStream);

        return Message(parser);
    }
}

TEST(GapProtoTest, round_trip_accepted_central_completion)
{
    gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::accepted }, gap::central::Completion::Result::timeout };

    EXPECT_EQ(completion, RoundTrip(completion));
}

TEST(GapProtoTest, round_trip_rejected_central_completion)
{
    gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::invalidState }, gap::central::Completion::Result::success };

    auto parsed = RoundTrip(completion);

    EXPECT_EQ(gap::central::RequestStatus::Status::invalidState, parsed.requestStatus.status);
    EXPECT_EQ(gap::central::Completion::Result::success, parsed.result);
}

TEST(GapProtoTest, central_completion_carries_every_gap_central_result)
{
    for (auto result : { gap::central::Completion::Result::success, gap::central::Completion::Result::cancelled,
             gap::central::Completion::Result::timeout, gap::central::Completion::Result::connectionFailed,
             gap::central::Completion::Result::controllerError })
    {
        gap::central::Completion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::accepted }, result };

        EXPECT_EQ(result, RoundTrip(completion).result);
    }
}

TEST(GapProtoTest, peripheral_completion_carries_every_gap_peripheral_result)
{
    for (auto result : { gap::peripheral::Completion::Result::success, gap::peripheral::Completion::Result::invalidParameter,
             gap::peripheral::Completion::Result::controllerError })
    {
        gap::peripheral::Completion completion{ gap::peripheral::RequestStatus{ gap::peripheral::RequestStatus::Status::accepted }, result };

        EXPECT_EQ(result, RoundTrip(completion).result);
    }
}

TEST(GapProtoTest, round_trip_pairing_completion)
{
    gap::peripheral::PairingCompletion completion{ gap::peripheral::RequestStatus{ gap::peripheral::RequestStatus::Status::accepted },
        gap::peripheral::PairingStatus{ gap::peripheral::PairingStatus::Result::numericComparisonFailed } };

    auto parsed = RoundTrip(completion);

    EXPECT_EQ(gap::peripheral::PairingStatus::Result::numericComparisonFailed, parsed.result.result);
}

TEST(GapProtoTest, round_trip_bond_completion)
{
    gap::central::BondCompletion completion{ gap::central::RequestStatus{ gap::central::RequestStatus::Status::busy } };

    EXPECT_EQ(gap::central::RequestStatus::Status::busy, RoundTrip(completion).requestStatus.status);
}

TEST(GapProtoTest, round_trip_discovered_device_reports_advertising_event_type)
{
    std::array<uint8_t, 3> advertisingData{ 0x02, 0x01, 0x06 };
    gap::central::DiscoveredDevice device{
        gap::central::Address{ infra::MakeRange(std::array<uint8_t, 6>{ 0, 1, 2, 3, 4, 5 }) },
        gap::central::AddressType{ gap::central::AddressType::AddressTypeEnum::randomAddress },
        infra::MakeRange(advertisingData),
        -75,
        gap::central::AdvertisingEventType{ gap::central::AdvertisingEventType::AdvertisingEventTypeEnum::advDirectInd }
    };

    auto parsed = RoundTrip(device);

    EXPECT_EQ(gap::central::AdvertisingEventType::AdvertisingEventTypeEnum::advDirectInd, parsed.eventType.type);
    EXPECT_EQ(gap::central::AddressType::AddressTypeEnum::randomAddress, parsed.addressType.type);
    EXPECT_EQ(-75, parsed.rssi);
}

TEST(GapProtoTest, round_trip_device_address)
{
    gap::central::DeviceAddress deviceAddress{
        gap::central::Address{ infra::MakeRange(std::array<uint8_t, 6>{ 5, 4, 3, 2, 1, 0 }) },
        gap::central::AddressType{ gap::central::AddressType::AddressTypeEnum::publicAddress }
    };

    EXPECT_EQ(deviceAddress, RoundTrip(deviceAddress));
}

TEST(GapProtoTest, io_capabilities_match_the_security_manager_encoding)
{
    // Bluetooth Core Specification, Volume 3, Part H, section 3.3.1, Table 3.4.
    // Asserted against the specification's own numbers, not merely against each other:
    // two sides agreeing on a wrong encoding is the failure this guards.
    EXPECT_EQ(0x00u, infra::enum_cast(services::GapPairing::IoCapabilities::display));
    EXPECT_EQ(0x01u, infra::enum_cast(services::GapPairing::IoCapabilities::displayYesNo));
    EXPECT_EQ(0x02u, infra::enum_cast(services::GapPairing::IoCapabilities::keyboard));
    EXPECT_EQ(0x03u, infra::enum_cast(services::GapPairing::IoCapabilities::none));
    EXPECT_EQ(0x04u, infra::enum_cast(services::GapPairing::IoCapabilities::keyboardDisplay));

    EXPECT_EQ(0x00u, infra::enum_cast(gap::central::IoCapabilities::IoCapabilitiesEnum::display));
    EXPECT_EQ(0x01u, infra::enum_cast(gap::central::IoCapabilities::IoCapabilitiesEnum::displayYesNo));
    EXPECT_EQ(0x02u, infra::enum_cast(gap::central::IoCapabilities::IoCapabilitiesEnum::keyboard));
    EXPECT_EQ(0x03u, infra::enum_cast(gap::central::IoCapabilities::IoCapabilitiesEnum::none));
    EXPECT_EQ(0x04u, infra::enum_cast(gap::central::IoCapabilities::IoCapabilitiesEnum::keyboardDisplay));

    EXPECT_EQ(0x00u, infra::enum_cast(gap::peripheral::IoCapabilities::IoCapabilitiesEnum::display));
    EXPECT_EQ(0x01u, infra::enum_cast(gap::peripheral::IoCapabilities::IoCapabilitiesEnum::displayYesNo));
    EXPECT_EQ(0x02u, infra::enum_cast(gap::peripheral::IoCapabilities::IoCapabilitiesEnum::keyboard));
    EXPECT_EQ(0x03u, infra::enum_cast(gap::peripheral::IoCapabilities::IoCapabilitiesEnum::none));
    EXPECT_EQ(0x04u, infra::enum_cast(gap::peripheral::IoCapabilities::IoCapabilitiesEnum::keyboardDisplay));
}

TEST(GapProtoTest, pairing_result_matches_the_proto_on_every_value)
{
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::success),
        infra::enum_cast(gap::central::PairingStatus::Result::success));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::passkeyEntryFailed),
        infra::enum_cast(gap::central::PairingStatus::Result::passkeyEntryFailed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::authenticationRequirementsNotMet),
        infra::enum_cast(gap::central::PairingStatus::Result::authenticationRequirementsNotMet));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::pairingNotSupported),
        infra::enum_cast(gap::central::PairingStatus::Result::pairingNotSupported));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::insufficientEncryptionKeySize),
        infra::enum_cast(gap::central::PairingStatus::Result::insufficientEncryptionKeySize));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::numericComparisonFailed),
        infra::enum_cast(gap::central::PairingStatus::Result::numericComparisonFailed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::timeout),
        infra::enum_cast(gap::central::PairingStatus::Result::timeout));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::encryptionFailed),
        infra::enum_cast(gap::central::PairingStatus::Result::encryptionFailed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::unknown),
        infra::enum_cast(gap::central::PairingStatus::Result::unknown));
}

TEST(GapProtoTest, peripheral_pairing_result_matches_the_central_one)
{
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::success),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::success));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::passkeyEntryFailed),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::passkeyEntryFailed));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::authenticationRequirementsNotMet),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::authenticationRequirementsNotMet));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::pairingNotSupported),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::pairingNotSupported));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::insufficientEncryptionKeySize),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::insufficientEncryptionKeySize));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::numericComparisonFailed),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::numericComparisonFailed));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::timeout),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::timeout));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::encryptionFailed),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::encryptionFailed));
    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::unknown),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::unknown));
}

TEST(GapProtoTest, security_mode_and_level_matches_the_proto)
{
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode1Level1),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level1));
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode1Level2),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level2));
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode1Level3),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level3));
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode1Level4),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level4));
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode2Level1),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode2Level1));
    EXPECT_EQ(infra::enum_cast(services::GapPairing::SecurityModeAndLevel::mode2Level2),
        infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode2Level2));

    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level1),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode1Level1));
    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level2),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode1Level2));
    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level3),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode1Level3));
    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode1Level4),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode1Level4));
    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode2Level1),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode2Level1));
    EXPECT_EQ(infra::enum_cast(gap::central::SecurityModeAndLevel::ModeAndLevel::mode2Level2),
        infra::enum_cast(gap::peripheral::SecurityModeAndLevel::ModeAndLevel::mode2Level2));
}

TEST(GapProtoTest, the_security_mode_request_moved_to_a_new_method_id)
{
    EXPECT_EQ(19u, gap::central::GapCentralProxy::idSetSecurityMode);
    EXPECT_EQ(20u, gap::central::GapCentralProxy::idSetSecureConnectionsOnly);
    EXPECT_EQ(19u, gap::peripheral::GapPeripheralProxy::idSetSecurityMode);
    EXPECT_EQ(20u, gap::peripheral::GapPeripheralProxy::idSetSecureConnectionsOnly);

    EXPECT_EQ(18u, gap::central::GapCentralResponseProxy::idSetSecurityModeComplete);
    EXPECT_EQ(13u, gap::peripheral::GapPeripheralResponseProxy::idSetSecurityModeComplete);
}

TEST(GapProtoTest, pairing_result_carries_every_security_manager_failure)
{
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::oobNotAvailable),
        infra::enum_cast(gap::central::PairingStatus::Result::oobNotAvailable));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::confirmValueFailed),
        infra::enum_cast(gap::central::PairingStatus::Result::confirmValueFailed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::commandNotSupported),
        infra::enum_cast(gap::central::PairingStatus::Result::commandNotSupported));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::repeatedAttempts),
        infra::enum_cast(gap::central::PairingStatus::Result::repeatedAttempts));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::invalidParameters),
        infra::enum_cast(gap::central::PairingStatus::Result::invalidParameters));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::dhKeyCheckFailed),
        infra::enum_cast(gap::central::PairingStatus::Result::dhKeyCheckFailed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::brEdrPairingInProgress),
        infra::enum_cast(gap::central::PairingStatus::Result::brEdrPairingInProgress));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::crossTransportKeyDerivationNotAllowed),
        infra::enum_cast(gap::central::PairingStatus::Result::crossTransportKeyDerivationNotAllowed));
    EXPECT_EQ(infra::enum_cast(services::GapPairingResult::keyRejected),
        infra::enum_cast(gap::central::PairingStatus::Result::keyRejected));

    EXPECT_EQ(infra::enum_cast(gap::central::PairingStatus::Result::keyRejected),
        infra::enum_cast(gap::peripheral::PairingStatus::Result::keyRejected));
}

TEST(GapProtoTest, the_established_pairing_results_did_not_move)
{
    EXPECT_EQ(0u, infra::enum_cast(services::GapPairingResult::success));
    EXPECT_EQ(8u, infra::enum_cast(services::GapPairingResult::unknown));
    EXPECT_EQ(9u, infra::enum_cast(services::GapPairingResult::oobNotAvailable));
    EXPECT_EQ(17u, infra::enum_cast(services::GapPairingResult::keyRejected));
}

TEST(GapProtoTest, advertisement_type_matches_the_proto)
{
    EXPECT_EQ(infra::enum_cast(services::GapAdvertisementType::advInd),
        infra::enum_cast(gap::peripheral::AdvertisementType::AdvertisementTypeEnum::advInd));
    EXPECT_EQ(infra::enum_cast(services::GapAdvertisementType::advNonconnInd),
        infra::enum_cast(gap::peripheral::AdvertisementType::AdvertisementTypeEnum::advNonconnInd));
    EXPECT_EQ(infra::enum_cast(services::GapAdvertisementType::advScanInd),
        infra::enum_cast(gap::peripheral::AdvertisementType::AdvertisementTypeEnum::advScanInd));

    EXPECT_EQ(infra::enum_cast(services::GapDirectedAdvertisementType::highDutyCycle),
        infra::enum_cast(gap::peripheral::DirectedAdvertisementMode::DirectedAdvertisementTypeEnum::highDutyCycle));
    EXPECT_EQ(infra::enum_cast(services::GapDirectedAdvertisementType::lowDutyCycle),
        infra::enum_cast(gap::peripheral::DirectedAdvertisementMode::DirectedAdvertisementTypeEnum::lowDutyCycle));
}

TEST(GapProtoTest, advertise_kept_its_method_id)
{
    EXPECT_EQ(1u, gap::peripheral::GapPeripheralProxy::idAdvertise);
    EXPECT_EQ(21u, gap::peripheral::GapPeripheralProxy::idAdvertiseDirected);
}

TEST(GapProtoTest, scan_type_matches_the_proto)
{
    EXPECT_EQ(infra::enum_cast(services::GapScanType::passive),
        infra::enum_cast(gap::central::ScanParameters::ScanType::passive));
    EXPECT_EQ(infra::enum_cast(services::GapScanType::active),
        infra::enum_cast(gap::central::ScanParameters::ScanType::active));
}

TEST(GapProtoTest, round_trip_scan_parameters)
{
    gap::central::ScanParameters parameters{ 0x0100, 0x0050, gap::central::ScanParameters::ScanType::passive };

    auto parsed = RoundTrip(parameters);

    EXPECT_EQ(0x0100u, parsed.interval);
    EXPECT_EQ(0x0050u, parsed.window);
    EXPECT_EQ(gap::central::ScanParameters::ScanType::passive, parsed.type);
}

TEST(GapProtoTest, start_device_discovery_moved_to_a_new_method_id)
{
    EXPECT_EQ(21u, gap::central::GapCentralProxy::idStartDeviceDiscovery);

    EXPECT_EQ(12u, gap::central::GapCentralResponseProxy::idStartDeviceDiscoveryComplete);
}

TEST(GapProtoTest, the_resolved_private_address_moved_to_a_new_method_id)
{
    EXPECT_EQ(27u, gap::central::GapCentralResponseProxy::idResolvedPrivateAddress);
}

TEST(GapProtoTest, round_trip_a_resolved_identity_address)
{
    gap::central::DeviceAddress identity{
        gap::central::Address{ infra::MakeRange(std::array<uint8_t, 6>{ 5, 4, 3, 2, 1, 0 }) },
        gap::central::AddressType{ gap::central::AddressType::AddressTypeEnum::publicAddress }
    };

    auto parsed = RoundTrip(identity);

    EXPECT_EQ(gap::central::AddressType::AddressTypeEnum::publicAddress, parsed.addressType.type);
    EXPECT_EQ(identity.address, parsed.address);
}

TEST(GapProtoTest, round_trip_a_bond_strength)
{
    gap::central::BondStrength strength{ true, true, true, 16 };

    auto parsed = RoundTrip(strength);

    EXPECT_EQ(strength, parsed);
    EXPECT_EQ(16u, parsed.encryptionKeySize);
}

TEST(GapProtoTest, a_bond_that_is_absent_is_carried_as_not_bonded)
{
    gap::central::BondStrength strength{ false, false, false, 0 };

    auto parsed = RoundTrip(strength);

    EXPECT_FALSE(parsed.bonded);
    EXPECT_EQ(strength, parsed);
}

TEST(GapProtoTest, the_successful_pairing_event_moved_to_a_new_method_id)
{
    EXPECT_EQ(29u, gap::central::GapCentralResponseProxy::idPairingSuccessfullyCompleted);
    EXPECT_EQ(31u, gap::peripheral::GapPeripheralResponseProxy::idPairingSuccessfullyCompleted);

    EXPECT_EQ(22u, gap::central::GapCentralProxy::idGetBondStrength);
    EXPECT_EQ(28u, gap::central::GapCentralResponseProxy::idBondStrengthReported);
    EXPECT_EQ(22u, gap::peripheral::GapPeripheralProxy::idGetBondStrength);
    EXPECT_EQ(30u, gap::peripheral::GapPeripheralResponseProxy::idBondStrengthReported);
}
