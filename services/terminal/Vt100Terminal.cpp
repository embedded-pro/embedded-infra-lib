#include "services/terminal/Vt100Terminal.hpp"
#include <array>
#include <string>
#include <utility>

namespace services
{
    namespace
    {
        int Param(const std::vector<int>& params, std::size_t index, int defaultValue)
        {
            if (index >= params.size())
                return defaultValue;
            int v = params[index];
            return v == 0 ? defaultValue : v;
        }

        int ParamRaw(const std::vector<int>& params, std::size_t index, int defaultValue)
        {
            if (index >= params.size())
                return defaultValue;
            return params[index];
        }

        struct ColorCode
        {
            int code;
            Color color;
        };

        using enum Color;

        constexpr std::array<ColorCode, 17> foregroundCodes{ {
            { 30, Black },
            { 31, Red },
            { 32, Green },
            { 33, Yellow },
            { 34, Blue },
            { 35, Magenta },
            { 36, Cyan },
            { 37, White },
            { 39, Default },
            { 90, BrightBlack },
            { 91, BrightRed },
            { 92, BrightGreen },
            { 93, BrightYellow },
            { 94, BrightBlue },
            { 95, BrightMagenta },
            { 96, BrightCyan },
            { 97, BrightWhite },
        } };

        constexpr std::array<ColorCode, 17> backgroundCodes{ {
            { 40, Black },
            { 41, Red },
            { 42, Green },
            { 43, Yellow },
            { 44, Blue },
            { 45, Magenta },
            { 46, Cyan },
            { 47, White },
            { 49, Default },
            { 100, BrightBlack },
            { 101, BrightRed },
            { 102, BrightGreen },
            { 103, BrightYellow },
            { 104, BrightBlue },
            { 105, BrightMagenta },
            { 106, BrightCyan },
            { 107, BrightWhite },
        } };

        bool ApplyColorCode(int parameter, std::span<const ColorCode> codes, Color& target)
        {
            for (const ColorCode& code : codes)
            {
                if (parameter == code.code)
                {
                    target = code.color;
                    return true;
                }
            }
            return false;
        }

        bool ApplyStyleCode(int parameter, Rendition& rendition)
        {
            if (parameter == 0)
                rendition = {};
            else if (parameter == 1)
                rendition.bold = true;
            else if (parameter == 2)
                rendition.faint = true;
            else if (parameter == 3)
                rendition.italic = true;
            else if (parameter == 4)
                rendition.underline = true;
            else if (parameter == 5)
                rendition.blink = true;
            else if (parameter == 7)
                rendition.inverse = true;
            else if (parameter == 22)
            {
                rendition.bold = false;
                rendition.faint = false;
            }
            else if (parameter == 23)
                rendition.italic = false;
            else if (parameter == 24)
                rendition.underline = false;
            else if (parameter == 25)
                rendition.blink = false;
            else if (parameter == 27)
                rendition.inverse = false;
            else
                return false;

            return true;
        }

        void ApplySgrParameter(int parameter, Rendition& rendition)
        {
            if (ApplyStyleCode(parameter, rendition))
                return;
            if (ApplyColorCode(parameter, foregroundCodes, rendition.foreground))
                return;
            ApplyColorCode(parameter, backgroundCodes, rendition.background);
        }

        void FillScreenWithAlignmentPattern(TerminalScreen& screen)
        {
            for (int row = 0; row < screen.Rows(); ++row)
            {
                for (int column = 0; column < screen.Cols(); ++column)
                {
                    screen.CursorOperations().MoveTo(row + 1, column + 1);
                    screen.Write(U'E');
                }
            }
            screen.CursorOperations().MoveTo(1, 1);
        }

        void ApplyPrivateMode(TerminalScreen& screen, int parameter, bool set)
        {
            if (parameter == 1)
                screen.GetModes().applicationCursorKeys = set;
            else if (parameter == 6)
                screen.GetModes().originMode = set;
            else if (parameter == 7)
                screen.GetModes().autoWrap = set;
            else if (parameter == 25)
                screen.GetModes().cursorVisible = set;
        }

        void ApplyAnsiMode(TerminalScreen& screen, int parameter, bool set)
        {
            if (parameter == 20)
                screen.GetModes().lineFeedNewLine = set;
        }
    }

    Vt100Terminal::Vt100Terminal(int rows, int cols)
        : screen(rows, cols)
        , parser(ParserCallbacks{
              [this](char32_t ch)
              {
                  OnPrint(ch);
              },
              [this](uint8_t b)
              {
                  OnExecute(b);
              },
              [this](char finalByte, char intermediate)
              {
                  OnEsc(finalByte, intermediate);
              },
              [this](char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate)
              {
                  OnCsi(finalByte, params, privateMarker, intermediate);
              },
              [this](const std::string& payload)
              {
                  OnOsc(payload);
              },
          })
    {
    }

    void Vt100Terminal::Feed(std::span<const uint8_t> data)
    {
        parser.Feed(data);
    }

    void Vt100Terminal::Feed(std::string_view data)
    {
        parser.Feed(data);
    }

    void Vt100Terminal::FeedByte(uint8_t b)
    {
        parser.FeedByte(b);
    }

    const TerminalScreen& Vt100Terminal::Screen() const
    {
        return screen;
    }

    TerminalScreen& Vt100Terminal::Screen()
    {
        return screen;
    }

    std::string Vt100Terminal::TakeOutgoing()
    {
        std::string out = std::move(outgoing);
        outgoing.clear();
        return out;
    }

    void Vt100Terminal::SetDeviceAttributesResponse(std::string response)
    {
        deviceAttributesResponse = std::move(response);
    }

    void Vt100Terminal::OnPrint(char32_t ch)
    {
        screen.Write(ch);
    }

    void Vt100Terminal::OnExecute(uint8_t b)
    {
        switch (b)
        {
            case 0x08:
                screen.Backspace();
                break;
            case 0x09:
                screen.HorizontalTab();
                break;
            case 0x0A:
            case 0x0B:
            case 0x0C:
                screen.LineFeed();
                break;
            case 0x0D:
                screen.CarriageReturn();
                break;
            case 0x07:
                break;
            case 0x00:
                break;
            case 0x05:
                break;
            case 0x11:
                break;
            case 0x13:
                break;
            default:
                break;
        }
    }

    void Vt100Terminal::OnEsc(char finalByte, char intermediate)
    {
        if (intermediate == '(' || intermediate == ')' || intermediate == '*' || intermediate == '+')
        {
            return;
        }
        if (intermediate == '#')
        {
            if (finalByte == '8')
                FillScreenWithAlignmentPattern(screen);
            return;
        }
        if (intermediate != 0)
            return;

        switch (finalByte)
        {
            case 'D':
                screen.Index();
                break;
            case 'E':
                screen.NextLine();
                break;
            case 'M':
                screen.ReverseIndex();
                break;
            case 'H':
                screen.TabStops().SetHere();
                break;
            case '7':
                screen.CursorOperations().Save();
                break;
            case '8':
                screen.CursorOperations().Restore();
                break;
            case 'c':
                screen.Reset();
                break;
            case '=':
                screen.GetModes().applicationKeypad = true;
                break;
            case '>':
                screen.GetModes().applicationKeypad = false;
                break;
            case 'Z':
                outgoing += deviceAttributesResponse;
                break;
            default:
                break;
        }
    }

    void Vt100Terminal::OnCsi(char finalByte, const std::vector<int>& params, bool privateMarker, char intermediate)
    {
        if (intermediate != 0)
            return;

        switch (finalByte)
        {
            case 'A':
                screen.CursorOperations().Up(Param(params, 0, 1));
                break;
            case 'B':
                screen.CursorOperations().Down(Param(params, 0, 1));
                break;
            case 'C':
                screen.CursorOperations().Forward(Param(params, 0, 1));
                break;
            case 'D':
                screen.CursorOperations().Backward(Param(params, 0, 1));
                break;
            case 'E':
                screen.CursorOperations().Down(Param(params, 0, 1));
                screen.CursorOperations().MoveToColumn(1);
                break;
            case 'F':
                screen.CursorOperations().Up(Param(params, 0, 1));
                screen.CursorOperations().MoveToColumn(1);
                break;
            case 'G':
                screen.CursorOperations().MoveToColumn(Param(params, 0, 1));
                break;
            case 'H':
            case 'f':
                screen.CursorOperations().MoveTo(Param(params, 0, 1), Param(params, 1, 1));
                break;
            case 'J':
                screen.EraseInDisplay(ParamRaw(params, 0, 0));
                break;
            case 'K':
                screen.EraseInLine(ParamRaw(params, 0, 0));
                break;
            case 'g':
                if (ParamRaw(params, 0, 0) == 3)
                    screen.TabStops().ClearAll();
                else
                    screen.TabStops().ClearHere();
                break;
            case 'h':
                ApplyMode(params, true, privateMarker);
                break;
            case 'l':
                ApplyMode(params, false, privateMarker);
                break;
            case 'm':
                ApplySgr(params);
                break;
            case 'n':
                if (ParamRaw(params, 0, 0) == 5)
                    ReportDeviceStatus();
                else if (ParamRaw(params, 0, 0) == 6)
                    ReportCursorPosition();
                break;
            case 'c':
                if (!privateMarker)
                    ReportDeviceAttributes();
                break;
            case 'r':
                screen.SetScrollRegion(ParamRaw(params, 0, 1), ParamRaw(params, 1, screen.Rows()));
                break;
            case 's':
                screen.CursorOperations().Save();
                break;
            case 'u':
                screen.CursorOperations().Restore();
                break;
            default:
                break;
        }
    }

    void Vt100Terminal::OnOsc(const std::string& /*payload*/) const
    {
    }

    void Vt100Terminal::ApplySgr(const std::vector<int>& params)
    {
        Rendition r = screen.CurrentRendition();
        if (params.empty())
        {
            r = {};
            screen.SetRendition(r);
            return;
        }

        for (int parameter : params)
            ApplySgrParameter(parameter, r);
        screen.SetRendition(r);
    }

    void Vt100Terminal::ApplyMode(const std::vector<int>& params, bool set, bool privateMarker)
    {
        for (int p : params)
        {
            if (privateMarker)
                ApplyPrivateMode(screen, p, set);
            else
                ApplyAnsiMode(screen, p, set);
        }
    }

    void Vt100Terminal::ReportCursorPosition()
    {
        const auto& c = screen.Cursor();
        outgoing += "\x1B[";
        outgoing += std::to_string(c.row + 1);
        outgoing += ";";
        outgoing += std::to_string(c.column + 1);
        outgoing += "R";
    }

    void Vt100Terminal::ReportDeviceStatus()
    {
        outgoing += "\x1B[0n";
    }

    void Vt100Terminal::ReportDeviceAttributes()
    {
        outgoing += deviceAttributesResponse;
    }
}