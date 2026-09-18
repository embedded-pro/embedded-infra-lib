#include "services/ble/LinkConfiguringGapCentral.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    LinkConfiguringGapCentral::LinkConfiguringGapCentral(GapCentral& gapCentral, const Configuration& configuration)
        : GapCentralDecorator(gapCentral)
        , configuration(configuration)
    {}

    void LinkConfiguringGapCentral::StateChanged(GapCentralState state)
    {
        GapCentralDecorator::StateChanged(state);

        // The state change is reported from within the controller's event handling, which is no
        // place to start a new procedure, so the link is configured from the event dispatcher.
        if (state == GapCentralState::connected)
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    ConfigureLink();
                });
    }

    void LinkConfiguringGapCentral::ConfigureLink()
    {
        SetPhy(configuration.txPhy, configuration.rxPhy, [](Result) {});
        SetDataLength(configuration.dataLength, [](Result) {});
    }
}
