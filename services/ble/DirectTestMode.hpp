#ifndef SERVICES_DIRECT_TEST_MODE_HPP
#define SERVICES_DIRECT_TEST_MODE_HPP

#include "infra/util/Function.hpp"
#include <cstdint>

namespace services
{
    // Bluetooth Core Specification, Volume 6, Part F
    class DirectTestMode
    {
    public:
        // A test distinguishes the two LE Coded coding schemes, where a connection only selects
        // a PHY.
        // Bluetooth Core Specification, Volume 4, Part E, sections 7.8.28 and 7.8.29
        enum class Phy : uint8_t
        {
            le1M = 0x01u,
            le2M = 0x02u,
            leCodedS8 = 0x03u,
            leCodedS2 = 0x04u
        };

        // Bluetooth Core Specification, Volume 6, Part F, section 4.1.4
        enum class PacketPayload : uint8_t
        {
            pseudoRandom9 = 0x00u,
            alternating11110000 = 0x01u,
            alternating10101010 = 0x02u,
            pseudoRandom15 = 0x03u,
            allOnes = 0x04u,
            allZeroes = 0x05u,
            alternating00001111 = 0x06u,
            alternating0101 = 0x07u
        };

        enum class RequestStatus : uint8_t
        {
            accepted = 0,
            invalidState,
            invalidParameter,
            busy,
            notSupported
        };

        enum class Result : uint8_t
        {
            success = 0,
            invalidParameter,
            controllerError
        };

        // Channel N, where the centre frequency is 2402 + 2 * N MHz.
        // Bluetooth Core Specification, Volume 6, Part F, section 4.1.1
        static constexpr uint8_t channelMax = 39;

        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.28
        virtual RequestStatus StartReceiverTest(uint8_t channel, Phy phy, const infra::Function<void(Result)>& onDone) = 0;

        // dataLength is the number of payload octets in each packet.
        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.29
        virtual RequestStatus StartTransmitterTest(uint8_t channel, uint8_t dataLength, PacketPayload payload, Phy phy, const infra::Function<void(Result)>& onDone) = 0;

        // The count is of packets received during a receiver test, and is zero after a
        // transmitter test.
        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.30
        virtual RequestStatus EndTest(const infra::Function<void(Result, uint16_t packetsReceived)>& onDone) = 0;

        // An unmodulated carrier is not part of Direct Test Mode. Controllers expose it through
        // vendor-specific commands, for the radiated measurements that reference packets cannot
        // serve; offset shifts the carrier within the channel.
        virtual RequestStatus StartUnmodulatedCarrier(uint8_t channel, uint8_t offset, const infra::Function<void(Result)>& onDone) = 0;
        virtual RequestStatus StopUnmodulatedCarrier(const infra::Function<void(Result)>& onDone) = 0;

        // Transmit power in dBm, as LE Transmitter Test v4 carries it.
        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.29
        virtual RequestStatus SetTransmitPowerLevel(int8_t txPower, const infra::Function<void(Result)>& onDone) = 0;
    };
}

#endif
