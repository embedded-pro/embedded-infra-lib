#ifndef SERVICES_NETWORK_ECHO_ECHO_INSTANTIATION_ON_NETWORK_HPP
#define SERVICES_NETWORK_ECHO_ECHO_INSTANTIATION_ON_NETWORK_HPP

#include "services/echo_core/TracingEcho.hpp"
#include "services/network/echo/EchoOnConnection.hpp"
#include "services/network/echo/TracingEchoOnConnection.hpp"
#include "services/network/http/HttpClientImpl.hpp"
#include "services/network/websocket/WebSocketClientConnectionObserver.hpp"

namespace application
{
    std::shared_ptr<services::Echo> OpenEcho(infra::BoundedString target, services::ConnectionFactoryWithNameResolver& connectionFactory);
    std::pair<std::shared_ptr<services::Echo>, std::shared_ptr<services::TracingEchoOnStreams>> OpenTracingEcho(infra::BoundedString target, services::ConnectionFactoryWithNameResolver& connectionFactory, services::Tracer& tracer);

    class EchoClientWebSocket
        : protected services::WebSocketClientObserverFactory
    {
    public:
        using OnDoneType = infra::Function<void(services::Echo&), 2 * sizeof(void*) + sizeof(std::shared_ptr<void>)>;

        EchoClientWebSocket(services::ConnectionFactoryWithNameResolver& connectionFactory,
            hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::BoundedString url, uint16_t port = 80);

        void OnDone(const OnDoneType& onDone);

    protected:
        infra::BoundedString Url() const override;
        uint16_t Port() const override;
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver> client)>&& createdClientObserver) override;
        void ConnectionFailed(ConnectFailReason reason) override;

    private:
        infra::BoundedString url;
        uint16_t port;
        services::HttpClientConnectorWithNameResolverImpl<> clientConnector;
        infra::Creator<services::Stoppable, services::HttpClientWebSocketInitiation, void(services::WebSocketClientObserverFactory& clientObserverFactory, services::HttpClientWebSocketInitiationResult& result, hal::SynchronousRandomDataGenerator& randomDataGenerator)> httpClientInitiationCreator;
        services::WebSocketClientFactorySingleConnection webSocketFactory;

        infra::SharedOptional<services::EchoOnConnection> echoConnection;
        infra::SharedPtr<services::EchoOnConnection> echoConnectionPtr;
        OnDoneType onDone;
        services::MethodSerializerFactory::OnHeap serializerFactory;
    };

    class TracingEchoClientWebSocket
        : public EchoClientWebSocket
    {
    public:
        using OnDoneType = infra::Function<void(services::Echo&, services::TracingEchoOnStreams&), 2 * sizeof(void*) + 2 * sizeof(std::shared_ptr<void>)>;

        TracingEchoClientWebSocket(services::ConnectionFactoryWithNameResolver& connectionFactory,
            hal::SynchronousRandomDataGenerator& randomDataGenerator, infra::BoundedString url, uint16_t port, services::Tracer& tracer);

        void OnDone(const OnDoneType& onDone);

    protected:
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver> client)>&& createdClientObserver) override;

    private:
        services::Tracer& tracer;
        infra::SharedOptional<services::TracingEchoOnConnection> echoConnection;
        infra::SharedPtr<services::TracingEchoOnConnection> echoConnectionPtr;
        OnDoneType onDone;
        services::MethodSerializerFactory::OnHeap serializerFactory;
    };

    class EchoClientTcp
        : protected services::ClientConnectionObserverFactoryWithNameResolver
    {
    public:
        using OnDoneType = infra::Function<void(services::Echo&), 2 * sizeof(void*) + sizeof(std::shared_ptr<void>)>;

        EchoClientTcp(services::ConnectionFactoryWithNameResolver& connectionFactory, infra::BoundedConstString hostname, uint16_t port);

        void OnDone(const OnDoneType& onDone);

    protected:
        // Implementation of ClientConnectionObserverFactoryWithNameResolver
        infra::BoundedConstString Hostname() const override;
        uint16_t Port() const override;
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver> connectionObserver)>&& createdObserver) override;
        void ConnectionFailed(services::ClientConnectionObserverFactoryWithNameResolver::ConnectFailReason reason) override;

    private:
        infra::BoundedConstString hostname;
        uint16_t port;

        infra::SharedOptional<services::EchoOnConnection> echoConnection;
        infra::SharedPtr<services::EchoOnConnection> echoConnectionPtr;
        OnDoneType onDone;
        services::MethodSerializerFactory::OnHeap serializerFactory;
    };

    class TracingEchoClientTcp
        : public EchoClientTcp
    {
    public:
        using OnDoneType = infra::Function<void(services::Echo&, services::TracingEchoOnStreams&), 2 * sizeof(void*) + 2 * sizeof(std::shared_ptr<void>)>;

        TracingEchoClientTcp(services::ConnectionFactoryWithNameResolver& connectionFactory, infra::BoundedConstString hostname, uint16_t port, services::Tracer& tracer);

        void OnDone(const OnDoneType& onDone);

    protected:
        void ConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver> connectionObserver)>&& createdObserver) override;

    private:
        services::Tracer& tracer;
        infra::SharedOptional<services::TracingEchoOnConnection> echoConnection;
        infra::SharedPtr<services::TracingEchoOnConnection> echoConnectionPtr;
        OnDoneType onDone;
        services::MethodSerializerFactory::OnHeap serializerFactory;
    };
}

#endif
