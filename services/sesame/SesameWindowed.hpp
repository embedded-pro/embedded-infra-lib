#ifndef SERVICES_SESAME_WINDOWED_HPP
#define SERVICES_SESAME_WINDOWED_HPP

#include "infra/stream/LimitedInputStream.hpp"
#include "infra/util/Aligned.hpp"
#include "infra/util/Endian.hpp"
#include "infra/util/PolymorphicVariant.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/sesame/Sesame.hpp"
#include "services/util/WindowedProtocolTypes.hpp"

namespace services
{
    class SesameWindowed
        : public Sesame
        , private SesameEncodedObserver
    {
    public:
        explicit SesameWindowed(SesameEncoded& delegate);

        void Stop();

        // Implementation of Sesame
        void RequestSendMessage(std::size_t size) override;
        std::size_t MaxSendMessageSize() const override;
        void Reset() override;

    protected:
        // clang-format off
        virtual void ReceivedInit(uint16_t newWindow) {}
        virtual void ReceivedInitResponse(uint16_t newWindow) {}
        virtual void ReceivedReleaseWindow(uint16_t oldWindow, uint16_t newWindow) {}
        virtual void ForwardingReceivedMessage(infra::StreamReaderWithRewinding& reader) {}
        virtual void SendingInit(uint16_t newWindow) {}
        virtual void SendingInitResponse(uint16_t newWindow) {}
        virtual void SendingReleaseWindow(uint16_t deltaWindow) {}
        virtual void SendingMessage(infra::StreamWriter& writer) {}
        virtual void SettingOperational(std::optional<std::size_t> requestedSize, uint16_t releasedWindow, uint16_t otherWindow) {}

        // clang-format on

    private:
        // Implementation of SesameEncodedObserver
        void Initialized() override;
        void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        void MessageSent(std::size_t encodedSize) override;
        void ReceivedMessage(infra::SharedPtr<infra::StreamReaderWithRewinding>&& reader, std::size_t encodedSize) override;

    private:
        void ReceivedInitialize();
        void ForwardReceivedMessage(uint16_t encodedSize);
        void SetNextState();

    private:
        using Operation = WindowedProtocolOperation;
        using PacketInit = WindowedProtocolPacketInit;
        using PacketInitResponse = WindowedProtocolPacketInitResponse;
        using PacketReleaseWindow = WindowedProtocolPacketReleaseWindow;

    public:
        template<std::size_t MaxMessageSize, template<std::size_t> class MessageSize>
        static constexpr std::size_t bufferSizeForMessage = MessageSize<sizeof(Operation) + MaxMessageSize>::size * 2 + MessageSize<sizeof(PacketReleaseWindow)>::size;

    private:
        class State
        {
        public:
            explicit State(SesameWindowed& communication);
            virtual ~State() = default;

            virtual void Request();
            virtual void RequestSendMessage(std::size_t size);
            virtual void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer);
            virtual void MessageSent(std::size_t encodedSize);

        protected:
            SesameWindowed& communication;
        };

        class StateSendingInit
            : public State
        {
        public:
            explicit StateSendingInit(SesameWindowed& communication);

            void Request() override;
            void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
            void MessageSent(std::size_t encodedSize) override;
        };

        class StateSendingInitResponse
            : public State
        {
        public:
            explicit StateSendingInitResponse(SesameWindowed& communication);

            void Request() override;
            void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        };

        class StateOperational
            : public State
        {
        public:
            explicit StateOperational(SesameWindowed& communication);

            void RequestSendMessage(std::size_t size) override;
        };

        class StateSendingMessage
            : public State
        {
        public:
            explicit StateSendingMessage(SesameWindowed& communication);

            void Request() override;
            void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
            void MessageSent(std::size_t encodedSize) override;

        private:
            std::size_t requestedSize;
        };

        class StateSendingReleaseWindow
            : public State
        {
        public:
            explicit StateSendingReleaseWindow(SesameWindowed& communication);

            void Request() override;
            void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        };

    private:
        const uint16_t ownBufferSize;
        const uint16_t releaseWindowSize;
        bool initialized = false;
        infra::SharedPtr<infra::StreamReaderWithRewinding> receivedMessageReader;
        infra::AccessedBySharedPtr readerAccess;
        uint16_t otherAvailableWindow{ 0 };
        uint16_t maxUsableBufferSize = 0;
        uint16_t releasedWindow{ 0 };
        bool sendInitResponse{ false };
        bool sending = false;
        std::optional<std::size_t> requestedSendMessageSize;
        infra::PolymorphicVariant<State, StateSendingInit, StateSendingInitResponse, StateOperational, StateSendingMessage, StateSendingReleaseWindow> state;
    };
}

#endif
