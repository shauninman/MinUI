#pragma once

#include "menu.hpp"

// based on https://github.com/josegonzalez/minui-keyboard/blob/main/minui-keyboard.c
// and converted to C++ under MIT:
// Copyright (C) 2025 Jose Diaz-Gonzalez
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

enum class ExitCode
{
    Uninitialized = -1,
    Success = 0,
    Error = 1,
    CancelButton = 2,
    MenuButton = 3,
    ActionButton = 4,
    InactionButton = 5,
    StartButton = 6,
    ParseError = 10,
    SerializeError = 11,
    Timeout = 124,
    KeyboardInterrupt = 130,
    Sigterm = 143,
};

// KeyboardState holds the keyboard-related state
struct KeyboardState
{
    bool display;             // whether to display the keyboard
    int row;                  // the current keyboard row
    int col;                  // the current keyboard column
    int layout;               // the current keyboard layout
    std::string current_text; // the text to display in the keyboard
    std::string initial_text; // the initial value of the text on keyboard entry
    std::string final_text;   // the final value of the text on keyboard exit
    std::string title;        // the title of the keyboard
};

// AppState holds the current state of the application
struct AppState
{
    bool redraw;                   // whether the screen needs to be redrawn
    bool quitting;                 // whether the app should exit
    ExitCode exit_code;            // the exit code to return
    struct KeyboardState keyboard; // current keyboard state
};

using KeyboardLayout = std::vector<std::vector<std::string>>;

class KeyboardPrompt : public MenuList
{
    AppState state{};

public:
    KeyboardPrompt(const std::string &title, MenuListCallback on_confirm = nullptr);
    ~KeyboardPrompt();

    void drawCustom(SDL_Surface *surface, const SDL_Rect &dst) override;

    InputReactionHint handleInput(int &dirty, int &quit) override;

private:
    // handle_keyboard_input interprets keyboard input events and mutates app state
    void handleKeyboardInput(AppState &state);

    // cursor_rescue ensures the cursor lands on a valid key and doesn't get lost in empty space
    void cursorRescue(AppState &state, const KeyboardLayout& current_layout, int num_rows);

    // count_row_length returns the number of non-empty characters in a keyboard row
    int countRowLength(const KeyboardLayout& current_layout, int row);

    // calculate_column_offset returns the offset between two rows
    int calculateColumnOffset(const KeyboardLayout& current_layout, int from_row, int to_row);

    // adjust_offset_exit_last_row adjusts offset when exiting the last row
    int adjustOffsetExitLastRow(int offset, int column);

    // adjust_offset_enter_last_row adjusts offset when entering the last row
    int adjustOffsetEnterLastRow(int offset, int col, int center);

    // get_current_layout returns the appropriate keyboard layout array based on the current state
    const KeyboardLayout& getCurrentLayout(const AppState &state);

    // draw_keyboard interprets the app state and draws it as a keyboard to the screen
    void drawKeyboard(SDL_Surface *screen, const AppState &state);
};