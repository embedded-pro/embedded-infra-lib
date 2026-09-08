#pragma once

#include "services/terminal/TerminalTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace services
{
    class TerminalScreen;

    class TerminalCursorOperations
    {
    public:
        explicit TerminalCursorOperations(TerminalScreen& screen);

        void Up(int n);
        void Down(int n);
        void Forward(int n);
        void Backward(int n);
        void MoveTo(int row, int col);
        void MoveToColumn(int col);
        void Save();
        void Restore();

    private:
        TerminalScreen& screen;
    };

    class TerminalTabStops
    {
    public:
        explicit TerminalTabStops(TerminalScreen& screen);

        void SetHere();
        void ClearHere();
        void ClearAll();

    private:
        TerminalScreen& screen;
    };
    class TerminalScreen
    {
    public:
        explicit TerminalScreen(int rows = 24, int cols = 100);

        int Rows() const;
        int Cols() const;
        void Reset();
        void SoftReset();

        const Cell& At(int row, int col) const;
        const CursorPosition& Cursor() const;
        const Rendition& CurrentRendition() const;
        void SetRendition(const Rendition& rendition);

        const Modes& GetModes() const;
        Modes& GetModes();
        const std::deque<std::vector<Cell>>& History() const;
        void ClearHistory();
        void Write(char32_t ch);
        void CarriageReturn();
        void LineFeed();
        void Backspace();
        void HorizontalTab();
        void Index();
        void NextLine();
        void ReverseIndex();
        TerminalTabStops TabStops();
        TerminalCursorOperations CursorOperations();
        void EraseInDisplay(int mode);
        void EraseInLine(int mode);
        void SetScrollRegion(int top, int bottom);
        int ScrollTop() const;
        int ScrollBottom() const;
        std::string LineText(int row) const;

    private:
        friend class TerminalCursorOperations;
        friend class TerminalTabStops;

        void ScrollUpInRegion(int n);
        void ScrollDownInRegion(int n);
        std::vector<Cell> MakeBlankRow() const;
        Cell MakeBlankCell() const;
        void ClampCursor();

        int rows{};
        int cols{};
        std::vector<std::vector<Cell>> grid;
        CursorPosition cursor{};
        CursorPosition savedCursor{};
        Rendition currentRendition{};
        Rendition savedRendition{};
        std::vector<uint8_t> tabStops;
        int scrollTop{ 0 };
        int scrollBottom{ 0 };
        bool pendingWrap{ false };
        Modes modes{};
        std::deque<std::vector<Cell>> history;
        std::size_t maxHistory{ 1000 };
    };
}