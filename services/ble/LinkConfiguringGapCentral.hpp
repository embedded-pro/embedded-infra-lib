#ifndef SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP
#define SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP

#include "services/ble/GapCentral.hpp"

namespace services
{
    // A connection starts on the LE 1M PHY with a 27 octet payload, which the LE Set PHY and
    // LE Set Data Length procedures raise. Neither is run by the controller on its own, so this
    // decorator runs both once a connection is established and leaves the GapCentral it
    // decorates with nothing but the procedures themselves.
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

    private:
        void ConfigureLink();

    private:
        Configuration configuration;
    };
}

#endif
