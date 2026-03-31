#ifndef SERVICES_ECHO_ON_MESSAGE_COMMUNICATION_HPP
#define SERVICES_ECHO_ON_MESSAGE_COMMUNICATION_HPP

#include "services/echo_core/EchoOnStreams.hpp"
#include "services/message_communication/MessageCommunication.hpp"

namespace services
{
    class EchoOnMessageCommunication
        : public EchoOnStreams
        , public MessageCommunicationObserver
    {
    public:
        EchoOnMessageCommunication(MessageCommunication& subject, services::MethodSerializerFactory& serializerFactory, const EchoErrorPolicy& errorPolicy = echoErrorPolicyAbortOnMessageFormatError);

        // Implementation of MessageCommunicationObserver
        void Initialized() override;
        void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        void ReceivedMessage(infra::SharedPtr<infra::StreamReaderWithRewinding>&& reader) override;

    protected:
        // Implementation of EchoOnStreams
        void RequestSendStream(std::size_t size) override;
    };
}

#endif
