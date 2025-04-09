#include "keyboardprompt.hpp"

constexpr int keyboardRows = 5;
constexpr int keyboardColumns = 14;
// keyboard_layout_lowercase is the default keyboard layout
KeyboardLayout keyboardLayoutLowercase = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "\0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\", "\0"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "\0", "\0", "\0"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

// keyboard_layout_uppercase is the uppercase keyboard layout
KeyboardLayout keyboardLayoutUppercase = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "\0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|", "\0"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "\0", "\0", "\0"},
    {"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

// keyboard_layout_special is the special keyboard layout
// note that some characters are not supported by the font in use by MinUI
// so we omit those characters from the layout
KeyboardLayout keyboardLayoutSpecial = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "\0"},
    {"{", "}", "|", "\\", "<", ">", "?", "\"", ";", ":", "[", "]", "\\", "\0"},
    {"±", "§", "¶", "©", "®", "™", "€", "£", "¥", "¢", "¤", "\0", "\0", "\0"},
    {"°", "•", "·", "†", "‡", "¬", "¦", "¡", "\0", "\0", "\0", "\0", "\0", "\0"},
    {"shift", "space", "enter", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\0"}};

KeyboardPrompt::KeyboardPrompt(const std::string &title, MenuListCallback on_confirm)
    : MenuList(MenuItemType::Custom, title, {}, nullptr, on_confirm)
{
    state = {
        .redraw = true,
        .quitting = false,
        .exit_code = ExitCode::Uninitialized,
        .keyboard = {
            .display = true,
            .row = 0,
            .col = 0,
            .layout = 0,
            .title = title}};
}

KeyboardPrompt::~KeyboardPrompt() {}

void KeyboardPrompt::drawCustom(SDL_Surface *surface, const SDL_Rect &dst)
{
    drawKeyboard(surface, state);

    // don't forget to reset the should_redraw flag
    state.redraw = false;
}

InputReactionHint KeyboardPrompt::handleInput(int &dirty, int &quit)
{
    if (PAD_justPressed(BTN_Y))
    {
        state.keyboard.final_text = state.keyboard.initial_text;
        state.quitting = true;
        state.exit_code = ExitCode::CancelButton;
        state.redraw = true;
        // why is this branch not exiting?
    }

    if (PAD_justPressed(BTN_MENU))
    {
        state.keyboard.final_text = state.keyboard.initial_text;
        state.redraw = false;
        state.quitting = true;
        state.exit_code = ExitCode::MenuButton;

        // todo: update quit and dirty flags
        return InputReactionHint::Exit;
    }

    handleKeyboardInput(state);
    dirty |= state.redraw;
    quit |= state.quitting;

    if (state.exit_code == ExitCode::CancelButton) {
        return InputReactionHint::Exit;
    }
    else if (state.exit_code == ExitCode::Success) {
        if(on_confirm) {
            MenuItem tmp{ListItemType::Button, state.keyboard.final_text, ""};
            return on_confirm(tmp);
        }
        return InputReactionHint::Exit;
    }
    else {
        return InputReactionHint::NoOp;
    }
}

void KeyboardPrompt::handleKeyboardInput(AppState &state)
{
    // redraw unless a key was not pressed
    state.redraw = true;

    // track current keyboard layout
    const auto &currentLayout = getCurrentLayout(state);

    int max_row = keyboardRows;
    int max_col = keyboardColumns;

    if (PAD_justRepeated(BTN_UP))
    {
        if (state.keyboard.row > 0)
        {
            int offset = calculateColumnOffset(currentLayout, state.keyboard.row, state.keyboard.row - 1);

            if (state.keyboard.row == max_row - 1)
                offset = adjustOffsetExitLastRow(offset, state.keyboard.col);
            state.keyboard.col += offset;
            state.keyboard.row--;
        }
        else
        {
            int offset = calculateColumnOffset(currentLayout, 0, max_row - 1);
            int row_length = countRowLength(currentLayout, 0);
            int center = (row_length - 1) / 2;

            if (!((row_length & 1) == 0 && state.keyboard.col == center - 1))
                offset = adjustOffsetEnterLastRow(offset, state.keyboard.col, center);
            state.keyboard.col += offset;
            state.keyboard.row = max_row - 1;
        }
        cursorRescue(state, currentLayout, max_row);
    }
    else if (PAD_justRepeated(BTN_DOWN))
    {
        if (state.keyboard.row < max_row - 1)
        {
            int offset = calculateColumnOffset(currentLayout, state.keyboard.row, state.keyboard.row + 1);
            int row_length = countRowLength(currentLayout, state.keyboard.row);
            int center = (row_length - 1) / 2;

            if (state.keyboard.row + 1 == max_row - 1 && (state.keyboard.col > center || (row_length & 1 && state.keyboard.col < center)))
            {
                offset = adjustOffsetEnterLastRow(offset, state.keyboard.col, center);
            }
            state.keyboard.col += offset;
            state.keyboard.row++;
        }
        else
        {
            int offset = calculateColumnOffset(currentLayout, max_row - 1, 0);

            offset = adjustOffsetExitLastRow(offset, state.keyboard.col);
            state.keyboard.col += offset;
            state.keyboard.row = 0;
        }
        cursorRescue(state, currentLayout, max_row);
    }
    else if (PAD_justRepeated(BTN_LEFT))
    {
        if (state.keyboard.col > 0)
        {
            state.keyboard.col--;
        }
        else
        {
            int last_col = 13;
            while (last_col >= 0 && currentLayout[state.keyboard.row][last_col].empty())
                last_col--;
            state.keyboard.col = last_col;
        }
    }
    else if (PAD_justRepeated(BTN_RIGHT))
    {
        const auto &next_key = currentLayout.at(state.keyboard.row).at(state.keyboard.col + 1);
        if (state.keyboard.col + 1 >= max_col || next_key.empty())
            state.keyboard.col = 0;
        else if (state.keyboard.col < max_col - 1)
            state.keyboard.col++;
    }
    else if (PAD_justPressed(BTN_X))
    {
        state.keyboard.final_text = state.keyboard.current_text;
        state.keyboard.display = !state.keyboard.display;
        state.redraw = true;
        state.quitting = true;
        state.exit_code = ExitCode::Success;
    }
    else if (PAD_justPressed(BTN_B))
    {
        if (!state.keyboard.current_text.empty())
            state.keyboard.current_text.pop_back();
    }
    else if (PAD_justPressed(BTN_A))
    {
        const auto &key = currentLayout.at(state.keyboard.row).at(state.keyboard.col);

        if (!key.empty())
        {
            if (key == "shift")
            {
                state.keyboard.layout = (state.keyboard.layout + 1) % 3;
            }
            else if (key == "space")
            {
                state.keyboard.current_text.append(" ");
            }
            else if (key == "enter")
            {
                state.keyboard.final_text = state.keyboard.current_text;
                state.keyboard.display = !state.keyboard.display;
                state.quitting = true;
                state.exit_code = ExitCode::Success;
            }
            else
            {
                state.keyboard.current_text.append(key);
            }
        }
    }
    else if (PAD_justPressed(BTN_SELECT))
    {
        state.keyboard.layout = (state.keyboard.layout + 1) % 3;
        const auto& nextLayout = getCurrentLayout(state);
        cursorRescue(state, nextLayout, max_row);
    }
    else
    {
        // do not redraw if no key was pressed
        state.redraw = false;
    }
}

void KeyboardPrompt::cursorRescue(AppState &state, const KeyboardLayout &currentLayout, int num_rows)
{
    int num_cols = currentLayout[0].size();

    // Ensure row doesn't exceed boundaries
    if (state.keyboard.row < 0)
        state.keyboard.row = 0;
    else if (state.keyboard.row > num_rows - 1)
        state.keyboard.row = num_rows - 1;

    // Ensure column doesn't exceed maximum
    if (state.keyboard.col > num_cols - 1)
        state.keyboard.col = num_cols - 1;

    // Move left until we find a non-empty cell
    while (state.keyboard.col >= 0 && currentLayout[state.keyboard.row][state.keyboard.col].empty())
        state.keyboard.col--;

    // If we went too far left, move right until we find a non-empty cell
    if (state.keyboard.col < 0)
    {
        state.keyboard.col = 0;
        while (currentLayout[state.keyboard.row][state.keyboard.col].empty())
            state.keyboard.col++;
    }
}

int KeyboardPrompt::countRowLength(const KeyboardLayout &layout, int row)
{
    int length = 0;
    for (int i = 0; i < keyboardColumns; i++)
        if (!layout[row][i].empty())
            length++;
    return length;
}

int KeyboardPrompt::calculateColumnOffset(const KeyboardLayout &layout, int from_row, int to_row)
{
    int from_length = countRowLength(layout, from_row);
    int to_length = countRowLength(layout, to_row);
    return (to_length - from_length) / 2;
}

int KeyboardPrompt::adjustOffsetExitLastRow(int offset, int column)
{
    if (column == 0)
        return offset - 1;
    else if (column == 2)
        return offset + 1;
    return offset; // Center stays fixed
}

int KeyboardPrompt::adjustOffsetEnterLastRow(int offset, int col, int center)
{
    if (col > center)
        return offset - 1;
    else if (col < center)
        return offset + 1;
    else
        return offset;
}

const KeyboardLayout &KeyboardPrompt::getCurrentLayout(const AppState &state)
{
    if (state.keyboard.layout == 0)
        return keyboardLayoutLowercase;
    else if (state.keyboard.layout == 1)
        return keyboardLayoutUppercase;
    else
        return keyboardLayoutSpecial;
}

void KeyboardPrompt::drawKeyboard(SDL_Surface *screen, const AppState &state)
{
    // determine which keyboard layout to use based on current state
    const KeyboardLayout *currentLayout = nullptr;
    if (state.keyboard.layout == 0)
        currentLayout = &keyboardLayoutLowercase;
    else if (state.keyboard.layout == 1)
        currentLayout = &keyboardLayoutUppercase;
    else
        currentLayout = &keyboardLayoutSpecial;
    const auto key = currentLayout->at(state.keyboard.row).at(state.keyboard.col);

    // draw the button group on the button-right
    char *hints[] = {(char *)("Y"), (char *)("EXIT"), (char *)("X"), ((char *)"ENTER"), NULL};
    GFX_blitButtonGroup(hints, 1, screen, 1);

    // draw keyboard title
    if (!state.keyboard.title.empty())
    {
        SDL_Surface *title = TTF_RenderUTF8_Blended(font.large, state.keyboard.title.c_str(), COLOR_WHITE);
        SDL_Rect title_pos = {
            (screen->w - title->w) / 2, // center horizontally
            20,                         // 20px from top
            title->w,
            title->h};
        SDL_BlitSurface(title, NULL, screen, &title_pos);
        SDL_FreeSurface(title);
    }

    // draw input field with current text
    // todo: use TTF_SizeUTF8 to compute the width of the input field
    SDL_Surface *input_placeholder = TTF_RenderUTF8_Blended(font.medium, "p", COLOR_WHITE);
    SDL_Surface *input = TTF_RenderUTF8_Blended(font.medium, state.keyboard.current_text.c_str(), COLOR_WHITE);
    SDL_Rect input_pos = {
        (screen->w) / 2,
        input_placeholder->h * 2,
        0,
        input_placeholder->h};
    if (input != NULL)
    {
        input_pos.x = (screen->w - input->w) / 2;
        input_pos.w = input->w;
        input_pos.h = input->h;
    }

    // draw input field background
    SDL_Rect input_bg = {
        40,
        input_placeholder->h * 2,
        screen->w - 80,
        input_placeholder->h};
    SDL_FillRect(screen, &input_bg, SDL_MapRGB(screen->format, TRIAD_DARK_GRAY));
    SDL_BlitSurface(input, NULL, screen, &input_pos);
    SDL_FreeSurface(input);

    // draw keyboard layout
    int start_y = input_placeholder->h * 4;
    int default_key_width = input_placeholder->w;
    int default_key_height = input_placeholder->h;
    int default_key_size = std::max(default_key_width, default_key_height);
    int row_spacing = 5;
    int column_spacing = 5;

    int num_rows = keyboardRows;
    int num_columns = keyboardColumns;

    // these special keys are not the same width as the other keys
    // so we need to compute their width separately
    // compute them here to avoid doing it conditionally for each row
    int shift_width, space_width, enter_width;
    TTF_SizeUTF8(font.medium, "shift", &shift_width, NULL);
    TTF_SizeUTF8(font.medium, "space", &space_width, NULL);
    TTF_SizeUTF8(font.medium, "enter", &enter_width, NULL);
    int special_key_width = std::max(shift_width, std::max(space_width, enter_width)) + (column_spacing * 4);

    for (int row = 0; row < num_rows; row++)
    {
        int len = 0;

        // Count non-null characters in the row
        for (int i = 0; i < num_columns; i++)
            if (!(*currentLayout)[row][i].empty())
                len++;

        int total_width = (len * default_key_size) + ((len - 1) * column_spacing); // 5px between keys
        if (row == 4)
        {
            // compute row 4 differently
            // row 4 has three buttons:
            // - "shift"
            // - "space"
            // - "enter"
            // so we need to account for the actual width of the buttons
            // we can use TTF_SizeUTF8 to get the width of the buttons
            // also we need to account for the padding between the keys
            // as well as the margin on each of the 3 keys
            total_width = (special_key_width * 3) + (2 * column_spacing);
        }
        int start_x = (screen->w - total_width) / 2;

        for (int col = 0; col < len; col++)
        {
            const auto& key = (*currentLayout)[row][col];
            if (key.empty())
                continue;

            SDL_Color text_color = (row == state.keyboard.row && col == state.keyboard.col) ? COLOR_BLACK : COLOR_WHITE;
            SDL_Surface *key_text = TTF_RenderUTF8_Blended(font.medium, key.c_str(), text_color);

            // special keys are not the same width as the other keys
            // so we need to compute their width separately
            int current_key_width = default_key_size;
            if (key == "shift" || key == "space" || key == "enter")
            {
                current_key_width = special_key_width;
            }

            SDL_Rect key_pos = {
                start_x + (col * (current_key_width + column_spacing)),
                start_y + (row * (default_key_size + row_spacing)),
                current_key_width,
                default_key_size};

            // draw key background
            Uint32 bg_color = (row == state.keyboard.row && col == state.keyboard.col) 
                ? SDL_MapRGB(screen->format, TRIAD_WHITE) 
                : SDL_MapRGB(screen->format, TRIAD_DARK_GRAY);
            SDL_FillRect(screen, &key_pos, bg_color);

            // center text in key
            SDL_Rect text_pos = {
                key_pos.x + (current_key_width - key_text->w) / 2,
                key_pos.y + (default_key_size - key_text->h) / 2,
                key_text->w,
                key_text->h};

            SDL_BlitSurface(key_text, NULL, screen, &text_pos);
            SDL_FreeSurface(key_text);
        }
    }
}
