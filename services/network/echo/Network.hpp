#ifndef SERVICES_NETWORK_HPP
#define SERVICES_NETWORK_HPP

#include "services/network/connection/Connection.hpp"
#include "services/network/connection/Datagram.hpp"
#include "services/network/connection/Multicast.hpp"
#include "services/network/connection/WiFiNetwork.hpp"

namespace main_
{
    struct Network
    {
        services::ConnectionFactory& connectionFactory;
        services::DatagramFactory& datagramFactory;
        services::Multicast& multicast;
        services::IpConfig ipConfig;
    };
}

#endif
