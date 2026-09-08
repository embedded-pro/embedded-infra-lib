#pragma once

#include "services/terminal/TerminalScreen.hpp"
#include "services/terminal/Vt100Parser.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace services::terminal
{
    class Vt100Terminal
    {
    public:
        explicit Vt100Terminal(int rows = 24, int cols = 100);

        void Feed(std::span<const uint8_t> data);
        void Feed(std::string_view data);
        void FeedByte(uint8_t b);

        const TerminalScreen& Screen() const;
        TerminalScreen& Screen();

        std::string TakeOutgoing();
        void SetDeviceAttributesResponse(std::string response);

    private:
        void OnPrint(char32_t ch);
        void OnExecute(uint8_t b);
        void OnEsc(char finalByte, char intermediate);
        void OnCsi(char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate);
        void OnOsc(const std::string& payload);

        void ApplySgr(const std::vector<int>& params);
        void ApplyMode(const std::vector<int>& params, bool set, bool privateMarker);
        void ReportCursorPosition();
        void ReportDeviceStatus();
        void ReportDeviceAttributes();

        TerminalScreen screen;
        Vt100Parser parser;
        std::string outgoing;
        std::string deviceAttributesResponse{ "\x1B[?6c" };
    };
}