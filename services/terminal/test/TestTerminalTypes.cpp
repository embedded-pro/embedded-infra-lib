#include "services/terminal/TerminalTypes.hpp"
#include <gtest/gtest.h>

TEST(TestTerminalTypes, rendition_defaults_to_plain_default_colors)
{
    const services::terminal::Rendition rendition;

    EXPECT_EQ(rendition.foreground, services::terminal::Color::Default);
    EXPECT_EQ(rendition.background, services::terminal::Color::Default);
    EXPECT_FALSE(rendition.bold);
    EXPECT_FALSE(rendition.faint);
    EXPECT_FALSE(rendition.italic);
    EXPECT_FALSE(rendition.underline);
    EXPECT_FALSE(rendition.blink);
    EXPECT_FALSE(rendition.inverse);
    EXPECT_EQ(rendition, services::terminal::Rendition{});
}

TEST(TestTerminalTypes, cell_defaults_to_blank_with_default_rendition)
{
    const services::terminal::Cell cell;

    EXPECT_EQ(cell.codepoint, U' ');
    EXPECT_EQ(cell.rendition, services::terminal::Rendition{});
    EXPECT_EQ(cell, services::terminal::Cell{});
}

TEST(TestTerminalTypes, cursor_position_defaults_to_origin)
{
    const services::terminal::CursorPosition cursor;

    EXPECT_EQ(cursor.row, 0);
    EXPECT_EQ(cursor.column, 0);
    EXPECT_EQ(cursor, services::terminal::CursorPosition{});
}

TEST(TestTerminalTypes, modes_default_to_vt100_power_on_state)
{
    const services::terminal::Modes modes;

    EXPECT_TRUE(modes.autoWrap);
    EXPECT_FALSE(modes.originMode);
    EXPECT_FALSE(modes.lineFeedNewLine);
    EXPECT_TRUE(modes.cursorVisible);
    EXPECT_FALSE(modes.applicationCursorKeys);
    EXPECT_FALSE(modes.applicationKeypad);
}