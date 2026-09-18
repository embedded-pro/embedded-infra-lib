#include "services/ble/GapCentral.hpp"

namespace services
{
    void GapCentralDecorator::DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered)
    {
        GapCentralObserver::SubjectType::NotifyObservers([&deviceDiscovered](auto& obs)
            {
                obs.DeviceDiscovered(deviceDiscovered);
            });
    }

    void GapCentralDecorator::StateChanged(GapCentralState state)
    {
        GapCentralObserver::SubjectType::NotifyObservers([&state](auto& obs)
            {
                obs.StateChanged(state);
            });
    }

    std::optional<GapAddress> GapCentralDecorator::ResolvePrivateAddress(hal::MacAddress address) const
    {
        return GapCentralObserver::Subject().ResolvePrivateAddress(address);
    }

    GapRequestStatus GapCentral::Connect(const GapAddress& peer, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone)
    {
        return Connect(peer, defaultConnectionParameters, initiatingTimeout, onDone);
    }

    GapRequestStatus GapCentralDecorator::Connect(const GapAddress& peer, const GapConnectionParameters& parameters, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().Connect(peer, parameters, initiatingTimeout, onDone);
    }

    GapRequestStatus GapCentralDecorator::UpdateConnectionParameters(const GapConnectionParameters& parameters, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().UpdateConnectionParameters(parameters, onDone);
    }

    GapRequestStatus GapCentralDecorator::CancelConnect(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().CancelConnect(onDone);
    }

    GapRequestStatus GapCentralDecorator::Disconnect(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().Disconnect(onDone);
    }

    GapRequestStatus GapCentralDecorator::SetAddress(const GapAddress& address, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().SetAddress(address, onDone);
    }

    GapRequestStatus GapCentralDecorator::SetDataLength(const GapDataLength& dataLength, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().SetDataLength(dataLength, onDone);
    }

    GapRequestStatus GapCentralDecorator::SetPhy(GapPhy txPhy, GapPhy rxPhy, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().SetPhy(txPhy, rxPhy, onDone);
    }

    GapRequestStatus GapCentral::StartDeviceDiscovery(const infra::Function<void(Result)>& onDone)
    {
        return StartDeviceDiscovery(defaultScanParameters, onDone);
    }

    GapRequestStatus GapCentralDecorator::StartDeviceDiscovery(const GapScanParameters& parameters, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().StartDeviceDiscovery(parameters, onDone);
    }

    GapRequestStatus GapCentralDecorator::StopDeviceDiscovery(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().StopDeviceDiscovery(onDone);
    }
}

namespace infra
{
    TextOutputStream& operator<<(TextOutputStream& stream, const services::GapCentralState& state)
    {
        if (state == services::GapCentralState::standby)
            stream << "Standby";
        else if (state == services::GapCentralState::scanning)
            stream << "Scanning";
        else if (state == services::GapCentralState::initiating)
            stream << "Initiating";
        else
            stream << "Connected";

        return stream;
    }
}
