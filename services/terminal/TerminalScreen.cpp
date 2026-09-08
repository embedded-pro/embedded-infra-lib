#include "services/terminal/TerminalScreen.hpp"
#include <algorithm>

namespace services::terminal
{
    namespace
    {
        constexpr int defaultTabInterval = 8;
    }

    TerminalScreen::TerminalScreen(int rows, int cols)
        : rows(rows > 0 ? rows : 24)
        , cols(cols > 0 ? cols : 100)
    {
        Reset();
    }

    int TerminalScreen::Rows() const
    {
        return rows;
    }

    int TerminalScreen::Cols() const
    {
        return cols;
    }

    void TerminalScreen::Reset()
    {
        currentRendition = {};
        savedRendition = {};
        cursor = {};
        savedCursor = {};
        modes = {};
        scrollTop = 0;
        scrollBottom = rows - 1;
        pendingWrap = false;

        grid.assign(rows, MakeBlankRow());
        tabStops.assign(cols, false);
        for (int c = defaultTabInterval; c < cols; c += defaultTabInterval)
            tabStops[c] = true;

        history.clear();
    }

    void TerminalScreen::SoftReset()
    {
        currentRendition = {};
        savedRendition = {};
        modes = {};
        scrollTop = 0;
        scrollBottom = rows - 1;
        pendingWrap = false;
        savedCursor = {};
    }

    const Cell& TerminalScreen::At(int row, int col) const
    {
        return grid[row][col];
    }

    const CursorPosition& TerminalScreen::Cursor() const
    {
        return cursor;
    }

    const Rendition& TerminalScreen::CurrentRendition() const
    {
        return currentRendition;
    }

    void TerminalScreen::SetRendition(const Rendition& rendition)
    {
        currentRendition = rendition;
    }

    const Modes& TerminalScreen::GetModes() const
    {
        return modes;
    }

    Modes& TerminalScreen::GetModes()
    {
        return modes;
    }

    const std::deque<std::vector<Cell>>& TerminalScreen::History() const
    {
        return history;
    }

    void TerminalScreen::ClearHistory()
    {
        history.clear();
    }

    Cell TerminalScreen::MakeBlankCell() const
    {
        return Cell{ U' ', currentRendition };
    }

    std::vector<Cell> TerminalScreen::MakeBlankRow() const
    {
        return std::vector<Cell>(static_cast<std::size_t>(cols), MakeBlankCell());
    }

    void TerminalScreen::ClampCursor()
    {
        if (cursor.row < 0)
            cursor.row = 0;
        if (cursor.row >= rows)
            cursor.row = rows - 1;
        if (cursor.column < 0)
            cursor.column = 0;
        if (cursor.column >= cols)
            cursor.column = cols - 1;
    }

    void TerminalScreen::Write(char32_t ch)
    {
        if (pendingWrap && modes.autoWrap)
        {
            CarriageReturn();
            Index();
        }
        pendingWrap = false;

        grid[cursor.row][cursor.column] = Cell{ ch, currentRendition };

        if (cursor.column + 1 >= cols)
        {
            pendingWrap = modes.autoWrap;
            cursor.column = cols - 1;
        }
        else
        {
            ++cursor.column;
        }
    }

    void TerminalScreen::CarriageReturn()
    {
        cursor.column = 0;
        pendingWrap = false;
    }

    void TerminalScreen::LineFeed()
    {
        Index();
        if (modes.lineFeedNewLine)
            CarriageReturn();
    }

    void TerminalScreen::Backspace()
    {
        if (cursor.column > 0)
            --cursor.column;
        pendingWrap = false;
    }

    void TerminalScreen::HorizontalTab()
    {
        pendingWrap = false;
        for (int c = cursor.column + 1; c < cols; ++c)
        {
            if (tabStops[c])
            {
                cursor.column = c;
                return;
            }
        }
        cursor.column = cols - 1;
    }

    void TerminalScreen::Index()
    {
        pendingWrap = false;
        if (cursor.row == scrollBottom)
            ScrollUpInRegion(1);
        else if (cursor.row < rows - 1)
            ++cursor.row;
    }

    void TerminalScreen::NextLine()
    {
        Index();
        CarriageReturn();
    }

    void TerminalScreen::ReverseIndex()
    {
        pendingWrap = false;
        if (cursor.row == scrollTop)
            ScrollDownInRegion(1);
        else if (cursor.row > 0)
            --cursor.row;
    }

    TerminalTabStops TerminalScreen::TabStops()
    {
        return TerminalTabStops{ *this };
    }

    TerminalCursorOperations TerminalScreen::CursorOperations()
    {
        return TerminalCursorOperations{ *this };
    }

    TerminalTabStops::TerminalTabStops(TerminalScreen& screen)
        : screen(screen)
    {
    }

    void TerminalTabStops::SetHere()
    {
        if (screen.cursor.column >= 0 && screen.cursor.column < screen.cols)
            screen.tabStops[screen.cursor.column] = true;
    }

    void TerminalTabStops::ClearHere()
    {
        if (screen.cursor.column >= 0 && screen.cursor.column < screen.cols)
            screen.tabStops[screen.cursor.column] = false;
    }

    void TerminalTabStops::ClearAll()
    {
        std::ranges::fill(screen.tabStops, false);
    }

    TerminalCursorOperations::TerminalCursorOperations(TerminalScreen& screen)
        : screen(screen)
    {
    }

    void TerminalCursorOperations::Up(int n)
    {
        screen.pendingWrap = false;
        if (n < 1)
            n = 1;
        screen.cursor.row = std::max(screen.scrollTop, screen.cursor.row - n);
    }

    void TerminalCursorOperations::Down(int n)
    {
        screen.pendingWrap = false;
        if (n < 1)
            n = 1;
        screen.cursor.row = std::min(screen.scrollBottom, screen.cursor.row + n);
    }

    void TerminalCursorOperations::Forward(int n)
    {
        screen.pendingWrap = false;
        if (n < 1)
            n = 1;
        screen.cursor.column = std::min(screen.cols - 1, screen.cursor.column + n);
    }

    void TerminalCursorOperations::Backward(int n)
    {
        screen.pendingWrap = false;
        if (n < 1)
            n = 1;
        screen.cursor.column = std::max(0, screen.cursor.column - n);
    }

    void TerminalCursorOperations::MoveTo(int row, int col)
    {
        screen.pendingWrap = false;
        if (row < 1)
            row = 1;
        if (col < 1)
            col = 1;
        screen.cursor.row = row - 1;
        screen.cursor.column = col - 1;
        screen.ClampCursor();
    }

    void TerminalCursorOperations::MoveToColumn(int col)
    {
        screen.pendingWrap = false;
        if (col < 1)
            col = 1;
        screen.cursor.column = std::min(screen.cols - 1, col - 1);
    }

    void TerminalCursorOperations::Save()
    {
        screen.savedCursor = screen.cursor;
        screen.savedRendition = screen.currentRendition;
    }

    void TerminalCursorOperations::Restore()
    {
        screen.cursor = screen.savedCursor;
        screen.currentRendition = screen.savedRendition;
        screen.pendingWrap = false;
        screen.ClampCursor();
    }

    void TerminalScreen::EraseInDisplay(int mode)
    {
        pendingWrap = false;
        const Cell blank = MakeBlankCell();
        auto eraseRow = [&](int row, int colFrom, int colTo)
        {
            for (int c = colFrom; c <= colTo; ++c)
                grid[row][c] = blank;
        };

        if (mode == 0)
        {
            eraseRow(cursor.row, cursor.column, cols - 1);
            for (int r = cursor.row + 1; r < rows; ++r)
                eraseRow(r, 0, cols - 1);
        }
        else if (mode == 1)
        {
            for (int r = 0; r < cursor.row; ++r)
                eraseRow(r, 0, cols - 1);
            eraseRow(cursor.row, 0, cursor.column);
        }
        else if (mode == 2 || mode == 3)
        {
            for (int r = 0; r < rows; ++r)
                eraseRow(r, 0, cols - 1);
            if (mode == 3)
                history.clear();
        }
    }

    void TerminalScreen::EraseInLine(int mode)
    {
        pendingWrap = false;
        const Cell blank = MakeBlankCell();
        if (mode == 0)
        {
            for (int c = cursor.column; c < cols; ++c)
                grid[cursor.row][c] = blank;
        }
        else if (mode == 1)
        {
            for (int c = 0; c <= cursor.column; ++c)
                grid[cursor.row][c] = blank;
        }
        else if (mode == 2)
        {
            for (int c = 0; c < cols; ++c)
                grid[cursor.row][c] = blank;
        }
    }

    void TerminalScreen::SetScrollRegion(int top, int bottom)
    {
        if (top < 1)
            top = 1;
        if (bottom < 1 || bottom > rows)
            bottom = rows;
        if (top >= bottom)
        {
            top = 1;
            bottom = rows;
        }
        scrollTop = top - 1;
        scrollBottom = bottom - 1;
        cursor.row = scrollTop;
        cursor.column = 0;
        pendingWrap = false;
    }

    int TerminalScreen::ScrollTop() const
    {
        return scrollTop;
    }

    int TerminalScreen::ScrollBottom() const
    {
        return scrollBottom;
    }

    void TerminalScreen::ScrollUpInRegion(int n)
    {
        if (n < 1)
            n = 1;
        const int regionRows = scrollBottom - scrollTop + 1;
        if (n > regionRows)
            n = regionRows;
        const bool fullScreen = scrollTop == 0 && scrollBottom == rows - 1;
        for (int i = 0; i < n; ++i)
        {
            if (fullScreen)
            {
                history.push_back(grid[scrollTop]);
                while (history.size() > maxHistory)
                    history.pop_front();
            }

            for (int r = scrollTop; r < scrollBottom; ++r)
                grid[r] = std::move(grid[r + 1]);
            grid[scrollBottom] = MakeBlankRow();
        }
    }

    void TerminalScreen::ScrollDownInRegion(int n)
    {
        if (n < 1)
            n = 1;
        const int regionRows = scrollBottom - scrollTop + 1;
        if (n > regionRows)
            n = regionRows;

        for (int i = 0; i < n; ++i)
        {
            for (int r = scrollBottom; r > scrollTop; --r)
                grid[r] = std::move(grid[r - 1]);
            grid[scrollTop] = MakeBlankRow();
        }
    }

    std::string TerminalScreen::LineText(int row) const
    {
        std::string out;
        out.reserve(static_cast<std::size_t>(cols));
        for (int c = 0; c < cols; ++c)
        {
            char32_t ch = grid[row][c].codepoint;
            if (ch <= 0x7F)
                out.push_back(static_cast<char>(ch));
            else
                out.push_back('?');
        }
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        return out;
    }
}