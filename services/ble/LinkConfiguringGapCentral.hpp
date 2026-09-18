#ifndef SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP
#define SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP

#include "infra/timer/Timer.hpp"
#include "services/ble/GapCentral.hpp"

namespace services
{
    // A connection starts on the LE 1M PHY carrying 27 octets. The controller raises neither on
    // its own, so this decorator asks for the PHY once a connection is established and for the
    // data length once the link layer reports which PHY it settled on, because the time a payload
    // takes on air depends on the PHY carrying it.
    class LinkConfiguringGapCentral
        : public GapCentralDecorator
    {
    public:
        struct Configuration
        {
            GapPhy txPhy;
            GapPhy rxPhy;
            GapDataLength dataLength;
        };

        static constexpr Configuration defaultConfiguration{ GapPhy::le2M, GapPhy::le2M, GapDataLength::Maximum(GapPhy::le1M) };

        explicit LinkConfiguringGapCentral(GapCentral& gapCentral, const Configuration& configuration = defaultConfiguration);

        // Implementation of GapCentralObserver
        void StateChanged(GapCentralState state) override;
        void PhyUpdated(GapPhy txPhy, GapPhy rxPhy) override;

    private:
        void RaisePhy();

    private:
        Configuration configuration;

        // A request is issued from a timer and never from the notification that prompted it, which
        // arrives from within the controller's event handling. Cancelling on destruction is what
        // keeps a pending request from outliving this decorator.
        infra::TimerSingleShot phyRequest;
        infra::TimerSingleShot dataLengthRequest;
    };
}

#endif
