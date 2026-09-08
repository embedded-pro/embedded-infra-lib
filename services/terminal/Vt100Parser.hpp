#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace services
{
    struct ParserCallbacks
    {
        std::function<void(char32_t)> Print;
        std::function<void(uint8_t)> Execute;
        std::function<void(char finalByte, char intermediate)> EscDispatch;
        std::function<void(char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate)> CsiDispatch;
        std::function<void(const std::string& payload)> OscDispatch;
    };

    class Vt100Parser
    {
    public:
        explicit Vt100Parser(ParserCallbacks callbacks);

        void Feed(std::span<const uint8_t> data);
        void Feed(std::string_view data);
        void FeedByte(uint8_t b);

        void Reset();

    private:
        enum class State : uint8_t
        {
            Ground,
            Escape,
            CsiEntry,
            CsiParam,
            CsiIntermediate,
            OscString,
            OscStringEsc,
            Ignore,
        };

        void Transition(State next);
        void Anywhere(uint8_t b, bool& consumed);
        void HandleGround(uint8_t b) const;
        void HandleEscape(uint8_t b);
        void HandleCsiEntry(uint8_t b);
        void HandleCsiParam(uint8_t b);
        void HandleCsiIntermediate(uint8_t b);
        void HandleOsc(uint8_t b);
        void HandleOscEsc(uint8_t b);
        void DispatchCsi(char finalByte);

        ParserCallbacks callbacks;
        State state{ State::Ground };
        std::string params;
        std::string oscBuffer;
        char intermediate{ 0 };
        bool privateMarker{ false };
    };
}