#include "services/terminal/TerminalTypes.hpp"
#include <gtest/gtest.h>

TEST(TestTerminalTypes, rendition_defaults_to_plain_default_colors)
{
    const services::Rendition rendition;

    EXPECT_EQ(rendition.foreground, services::Color::Default);
    EXPECT_EQ(rendition.background, services::Color::Default);
    EXPECT_FALSE(rendition.bold);
    EXPECT_FALSE(rendition.faint);
    EXPECT_FALSE(rendition.italic);
    EXPECT_FALSE(rendition.underline);
    EXPECT_FALSE(rendition.blink);
    EXPECT_FALSE(rendition.inverse);
    EXPECT_EQ(rendition, services::Rendition{});
}

TEST(TestTerminalTypes, cell_defaults_to_blank_with_default_rendition)
{
    const services::Cell cell;

    EXPECT_EQ(cell.codepoint, U' ');
    EXPECT_EQ(cell.rendition, services::Rendition{});
    EXPECT_EQ(cell, services::Cell{});
}

TEST(TestTerminalTypes, cursor_position_defaults_to_origin)
{
    const services::CursorPosition cursor;

    EXPECT_EQ(cursor.row, 0);
    EXPECT_EQ(cursor.column, 0);
    EXPECT_EQ(cursor, services::CursorPosition{});
}

TEST(TestTerminalTypes, modes_default_to_vt100_power_on_state)
{
    const services::Modes modes;

    EXPECT_TRUE(modes.autoWrap);
    EXPECT_FALSE(modes.originMode);
    EXPECT_FALSE(modes.lineFeedNewLine);
    EXPECT_TRUE(modes.cursorVisible);
    EXPECT_FALSE(modes.applicationCursorKeys);
    EXPECT_FALSE(modes.applicationKeypad);
}