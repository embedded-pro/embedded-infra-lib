#include "services/ble/LinkConfiguringGapCentral.hpp"

namespace services
{
    LinkConfiguringGapCentral::LinkConfiguringGapCentral(GapCentral& gapCentral, const Configuration& configuration)
        : GapCentralDecorator(gapCentral)
        , configuration(configuration)
    {}

    void LinkConfiguringGapCentral::StateChanged(GapCentralState state)
    {
        GapCentralDecorator::StateChanged(state);

        if (state == GapCentralState::connected)
            phyRequest.Start(infra::Duration{}, [this]()
                {
                    RaisePhy();
                });
        else
        {
            phyRequest.Cancel();
            dataLengthRequest.Cancel();
        }
    }

    void LinkConfiguringGapCentral::PhyUpdated(GapPhy txPhy, GapPhy rxPhy)
    {
        GapCentralDecorator::PhyUpdated(txPhy, rxPhy);

        dataLengthRequest.Start(infra::Duration{}, [this]()
            {
                GapCentralDecorator::SetDataLength(configuration.dataLength);
            });
    }

    void LinkConfiguringGapCentral::RaisePhy()
    {
        // A controller that will not run the PHY update reports nothing, leaving the link on the
        // PHY it already has, where the longer payload is still worth asking for.
        if (GapCentralDecorator::SetPhy(configuration.txPhy, configuration.rxPhy) != GapRequestStatus::accepted)
            GapCentralDecorator::SetDataLength(configuration.dataLength);
    }
}
