#include "hal/qemu/cmsdk/LineAssembler.hpp"
#include "gtest/gtest.h"
#include <string_view>

namespace
{
    class LineAssemblerTest
        : public testing::Test
    {
    public:
        const char* Receive(std::string_view text)
        {
            const char* line = nullptr;

            for (const auto c : text)
                line = assembler.Receive(c);

            return line;
        }

        hal::LineAssembler::WithMaxLineLength<4> assembler;
    };
}

TEST_F(LineAssemblerTest, characters_without_an_end_of_line_complete_no_line)
{
    EXPECT_EQ(nullptr, Receive("abc"));
}

TEST_F(LineAssemblerTest, a_newline_completes_the_line)
{
    const auto* line = Receive("abc\n");

    ASSERT_NE(nullptr, line);
    EXPECT_STREQ("abc", line);
}

TEST_F(LineAssemblerTest, a_carriage_return_completes_the_line)
{
    const auto* line = Receive("abc\r");

    ASSERT_NE(nullptr, line);
    EXPECT_STREQ("abc", line);
}

TEST_F(LineAssemblerTest, an_empty_line_is_not_reported)
{
    EXPECT_EQ(nullptr, Receive("\n"));
}

TEST_F(LineAssemblerTest, a_carriage_return_and_newline_pair_reports_a_single_line)
{
    EXPECT_NE(nullptr, Receive("a\r"));
    EXPECT_EQ(nullptr, assembler.Receive('\n'));
}

TEST_F(LineAssemblerTest, characters_beyond_the_maximum_line_length_are_dropped)
{
    const auto* line = Receive("abcdef\n");

    ASSERT_NE(nullptr, line);
    EXPECT_STREQ("abcd", line);
}

TEST_F(LineAssemblerTest, a_completed_line_starts_the_next_line_empty)
{
    Receive("abc\n");

    const auto* line = Receive("de\n");

    ASSERT_NE(nullptr, line);
    EXPECT_STREQ("de", line);
}
