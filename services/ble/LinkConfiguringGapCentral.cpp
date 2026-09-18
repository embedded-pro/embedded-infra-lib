#include "services/ble/LinkConfiguringGapCentral.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace
{
    template<class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };

    template<class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;
}

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
            Start();
        else
            Abandon();
    }

    void LinkConfiguringGapCentral::Start()
    {
        Abandon();

        // A procedure whose completion the controller never reported still holds the storage. The
        // link is then left as the controller set it up, rather than asserting on this connection.
        if (!procedureStorage.Allocatable())
            return;

        procedure = procedureStorage.Emplace(*this, SettingPhy{});

        // The state change is reported from within the controller's event handling, which is no
        // place to start a procedure, so the first step waits for the event dispatcher.
        infra::EventDispatcher::Instance().Schedule([started = infra::WeakPtr<Procedure>(procedure)]()
            {
                if (auto running = started.lock(); running != nullptr)
                    running->gapCentral.Perform();
            });
    }

    void LinkConfiguringGapCentral::Abandon()
    {
        procedure = nullptr;
    }

    void LinkConfiguringGapCentral::Perform()
    {
        auto status = std::visit(Overloaded{ [this](SettingPhy)
                                     {
                                         return GapCentralDecorator::SetPhy(configuration.txPhy, configuration.rxPhy, StepCompletion());
                                     },
                                     [this](SettingDataLength)
                                     {
                                         return GapCentralDecorator::SetDataLength(configuration.dataLength, StepCompletion());
                                     } },
            procedure->step);

        // A step the controller refuses reports nothing, so the one after it starts here.
        if (status != GapRequestStatus::accepted)
            StepDone();
    }

    void LinkConfiguringGapCentral::StepDone()
    {
        // The data length follows the PHY, because the time a payload takes on air depends on the
        // PHY carrying it. Whether the PHY changed does not decide that: the payload is worth
        // asking for on the PHY the link ends up with either way.
        if (std::holds_alternative<SettingPhy>(procedure->step))
        {
            procedure->step = SettingDataLength{};
            Perform();
        }
        else
            Abandon();
    }

    infra::Function<void(GapCentral::Result)> LinkConfiguringGapCentral::StepCompletion() const
    {
        return [running = infra::WeakPtr<Procedure>(procedure)](Result)
        {
            if (auto alive = running.lock(); alive != nullptr)
                alive->gapCentral.StepDone();
        };
    }
}
