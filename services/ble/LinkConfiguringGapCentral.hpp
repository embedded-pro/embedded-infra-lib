#ifndef SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP
#define SERVICES_LINK_CONFIGURING_GAP_CENTRAL_HPP

#include "infra/util/SharedOptional.hpp"
#include "services/ble/GapCentral.hpp"
#include <variant>

namespace services
{
    // A connection starts on the LE 1M PHY carrying 27 octets. The controller raises neither on
    // its own and runs one procedure at a time, so this decorator walks them in order once a
    // connection is established.
    //
    // Its lifetime is the GapCentral's, not a connection's. A disconnect arrives on its own and is
    // safe, because a port completes its pending procedures before reporting standby, but the
    // GapCentral offers no way to withdraw a completion already handed to it, so destroying this
    // while a procedure is in flight is not.
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
        struct SettingPhy
        {};

        struct SettingDataLength
        {};

        using Step = std::variant<SettingPhy, SettingDataLength>;

        // Completions reach this through a WeakPtr, so one arriving after the connection is gone
        // finds nothing and is discarded.
        struct Procedure //NOSONAR
        {
            Procedure(LinkConfiguringGapCentral& gapCentral, const Step& step)
                : gapCentral(gapCentral)
                , step(step)
            {}

            LinkConfiguringGapCentral& gapCentral;
            Step step;
        };

        void Start();
        void Abandon();
        void Perform();
        void StepDone();

        infra::Function<void(Result)> StepCompletion() const;

    private:
        Configuration configuration;
        infra::SharedOptional<Procedure> procedureStorage;
        infra::SharedPtr<Procedure> procedure;
    };
}

#endif
