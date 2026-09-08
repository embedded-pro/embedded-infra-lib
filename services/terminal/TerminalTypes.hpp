#pragma once

#include <cstdint>

namespace services
{
    enum class Color : uint8_t
    {
        Default,
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,
        BrightBlack,
        BrightRed,
        BrightGreen,
        BrightYellow,
        BrightBlue,
        BrightMagenta,
        BrightCyan,
        BrightWhite,
    };

    struct Rendition
    {
        Color foreground{ Color::Default };
        Color background{ Color::Default };
        bool bold{ false };
        bool faint{ false };
        bool italic{ false };
        bool underline{ false };
        bool blink{ false };
        bool inverse{ false };

        bool operator==(const Rendition&) const = default;
    };

    struct Cell
    {
        char32_t codepoint{ U' ' };
        Rendition rendition{};

        bool operator==(const Cell&) const = default;
    };

    struct CursorPosition
    {
        int row{ 0 };
        int column{ 0 };

        bool operator==(const CursorPosition&) const = default;
    };

    struct Modes
    {
        bool autoWrap{ true };
        bool originMode{ false };
        bool lineFeedNewLine{ false };
        bool cursorVisible{ true };
        bool applicationCursorKeys{ false };
        bool applicationKeypad{ false };
    };
}