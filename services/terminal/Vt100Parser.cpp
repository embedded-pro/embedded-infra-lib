#include "services/terminal/Vt100Parser.hpp"

namespace services
{
    namespace
    {
        constexpr uint8_t ESC_BYTE = 0x1B;
        constexpr uint8_t CAN_BYTE = 0x18;
        constexpr uint8_t SUB_BYTE = 0x1A;
        constexpr uint8_t BEL_BYTE = 0x07;
        constexpr uint8_t DEL_BYTE = 0x7F;

        bool IsC0Execute(uint8_t b)
        {
            if (b == ESC_BYTE || b == CAN_BYTE || b == SUB_BYTE)
                return false;
            if (b <= 0x17)
                return true;
            if (b == 0x19)
                return true;
            if (b >= 0x1C && b <= 0x1F)
                return true;
            return false;
        }

        bool IsIntermediate(uint8_t b)
        {
            return b >= 0x20 && b <= 0x2F;
        }

        bool IsParameter(uint8_t b)
        {
            return (b >= 0x30 && b <= 0x39) || b == 0x3B || b == 0x3A;
        }

        bool IsPrivateMarker(uint8_t b)
        {
            return b >= 0x3C && b <= 0x3F;
        }

        bool IsFinal(uint8_t b)
        {
            return b >= 0x40 && b <= 0x7E;
        }
    }

    Vt100Parser::Vt100Parser(ParserCallbacks callbacks)
        : callbacks(std::move(callbacks))
    {
    }

    void Vt100Parser::Reset()
    {
        using enum State;

        state = Ground;
        params.clear();
        oscBuffer.clear();
        intermediate = 0;
        privateMarker = false;
    }

    void Vt100Parser::Transition(State next)
    {
        using enum State;

        state = next;
        if (next == CsiEntry)
        {
            params.clear();
            intermediate = 0;
            privateMarker = false;
        }
        else if (next == Escape)
        {
            intermediate = 0;
        }
        else if (next == OscString)
        {
            oscBuffer.clear();
        }
    }

    void Vt100Parser::Feed(std::span<const uint8_t> data)
    {
        for (uint8_t byte : data)
            FeedByte(byte);
    }

    void Vt100Parser::Feed(std::string_view data)
    {
        for (char c : data)
            FeedByte(static_cast<uint8_t>(c));
    }

    void Vt100Parser::Anywhere(uint8_t b, bool& consumed)
    {
        using enum State;

        consumed = true;
        if (b == ESC_BYTE)
        {
            Transition(Escape);
        }
        else if (b == CAN_BYTE || b == SUB_BYTE)
        {
            Transition(Ground);
        }
        else
        {
            consumed = false;
        }
    }

    void Vt100Parser::FeedByte(uint8_t b)
    {
        using enum State;
        if (state == OscString)
        {
            HandleOsc(b);
            return;
        }
        if (state == OscStringEsc)
        {
            HandleOscEsc(b);
            return;
        }

        bool consumed = false;
        Anywhere(b, consumed);
        if (consumed)
            return;

        switch (state)
        {
            case Ground:
                HandleGround(b);
                break;
            case Escape:
                HandleEscape(b);
                break;
            case CsiEntry:
                HandleCsiEntry(b);
                break;
            case CsiParam:
                HandleCsiParam(b);
                break;
            case CsiIntermediate:
                HandleCsiIntermediate(b);
                break;
            case Ignore:
                if (IsFinal(b))
                    Transition(Ground);
                break;
            case OscString:
            case OscStringEsc:
                break;
        }
    }

    void Vt100Parser::HandleGround(uint8_t b) const
    {
        if (b == DEL_BYTE)
            return;
        if (IsC0Execute(b))
        {
            if (callbacks.Execute)
                callbacks.Execute(b);
            return;
        }
        if (b >= 0x20)
        {
            if (callbacks.Print)
                callbacks.Print(static_cast<char32_t>(b));
        }
    }

    void Vt100Parser::HandleEscape(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks.Execute)
                callbacks.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsIntermediate(b))
        {
            intermediate = static_cast<char>(b);
            return;
        }
        if (b == '[')
        {
            Transition(CsiEntry);
            return;
        }
        if (b == ']')
        {
            Transition(OscString);
            return;
        }
        if (b == 'P' || b == 'X' || b == '^' || b == '_')
        {
            Transition(OscString);
            return;
        }
        if (b == '\\')
        {
            Transition(Ground);
            return;
        }
        if (IsFinal(b) || (b >= 0x30 && b <= 0x3F))
        {
            if (callbacks.EscDispatch)
                callbacks.EscDispatch(static_cast<char>(b), intermediate);
            Transition(Ground);
            return;
        }
    }

    void Vt100Parser::HandleCsiEntry(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks.Execute)
                callbacks.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsPrivateMarker(b))
        {
            privateMarker = b == '?';
            Transition(CsiParam);
            return;
        }
        if (IsParameter(b))
        {
            params.push_back(static_cast<char>(b == 0x3A ? 0x3B : b));
            Transition(CsiParam);
            return;
        }
        if (IsIntermediate(b))
        {
            intermediate = static_cast<char>(b);
            Transition(CsiIntermediate);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::HandleCsiParam(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks.Execute)
                callbacks.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsParameter(b))
        {
            params.push_back(static_cast<char>(b == 0x3A ? 0x3B : b));
            return;
        }
        if (IsIntermediate(b))
        {
            intermediate = static_cast<char>(b);
            Transition(CsiIntermediate);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::HandleCsiIntermediate(uint8_t b)
    {
        using enum State;

        if (IsC0Execute(b))
        {
            if (callbacks.Execute)
                callbacks.Execute(b);
            return;
        }
        if (b == DEL_BYTE)
            return;
        if (IsIntermediate(b))
        {
            intermediate = static_cast<char>(b);
            return;
        }
        if (IsFinal(b))
        {
            DispatchCsi(static_cast<char>(b));
            return;
        }
        Transition(Ignore);
    }

    void Vt100Parser::DispatchCsi(char finalByte)
    {
        using enum State;

        std::vector<int> values;
        int current = 0;
        bool hasDigit = false;
        for (char c : params)
        {
            if (c >= '0' && c <= '9')
            {
                current = current * 10 + (c - '0');
                hasDigit = true;
            }
            else if (c == ';')
            {
                values.push_back(hasDigit ? current : 0);
                current = 0;
                hasDigit = false;
            }
        }
        if (hasDigit || !params.empty())
            values.push_back(current);

        if (callbacks.CsiDispatch)
            callbacks.CsiDispatch(finalByte, values, privateMarker, intermediate);

        Transition(Ground);
    }

    void Vt100Parser::HandleOsc(uint8_t b)
    {
        using enum State;

        if (b == BEL_BYTE)
        {
            if (callbacks.OscDispatch)
                callbacks.OscDispatch(oscBuffer);
            Transition(Ground);
            return;
        }
        if (b == ESC_BYTE)
        {
            state = OscStringEsc;
            return;
        }
        if (b == CAN_BYTE || b == SUB_BYTE)
        {
            Transition(Ground);
            return;
        }
        oscBuffer.push_back(static_cast<char>(b));
    }

    void Vt100Parser::HandleOscEsc(uint8_t b)
    {
        using enum State;

        if (b == '\\')
        {
            if (callbacks.OscDispatch)
                callbacks.OscDispatch(oscBuffer);
            Transition(Ground);
            return;
        }
        Transition(Escape);
        FeedByte(b);
    }
}