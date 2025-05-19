#include "menu.hpp"

extern "C"
{
#include "defines.h"
#include "api.h"
#include "utils.h"
}

#include <shared_mutex>
typedef std::shared_mutex Lock;
typedef std::unique_lock< Lock >  WriteLock;
typedef std::shared_lock< Lock >  ReadLock;

///////////////////////////////////////////////////////////

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
                   const std::vector<std::any> &values, const std::vector<std::string> &labels,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset, 
                   MenuListCallback on_confirm, MenuList *submenu)
    : type(type), name(name), desc(desc), values(values), labels(labels),
      on_get(on_get), on_set(on_set), on_confirm(on_confirm), on_reset(on_reset),
      submenu(submenu)
{
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, const std::vector<std::any> &values,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : type(type), name(name), desc(desc), values(values), /*labels({}),*/
    on_get(on_get), on_set(on_set), on_confirm(on_confirm), on_reset(on_reset),
    submenu(submenu)
{
    generateDefaultLabels();
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, 
                   int min, int max, const std::string suffix,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : type(type), name(name), desc(desc),
      on_get(on_get), on_set(on_set), on_confirm(on_confirm), on_reset(on_reset),
      submenu(submenu)
{
    const int step = 1; // until we need it
    const int num = (max - min) / step + 1;
    for (int i = 0; i < num; i++)
        values.push_back(min + i * step);

    generateDefaultLabels(suffix);
    initSelection();
    assert(valueIdx >= 0);
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
    MenuListCallback on_confirm, MenuList *submenu)
    : MenuItem(type, name, desc, 0,0, "", nullptr, nullptr, nullptr, on_confirm, submenu)
{
    
}

MenuItem::~MenuItem()
{
    // delete submenu;
}

void MenuItem::generateDefaultLabels(const std::string& suffix)
{
    labels.clear();
    for (auto v : values)
    {
        if (v.type() == typeid(std::string))
            labels.push_back(std::any_cast<std::string>(v) + suffix);
        else if (v.type() == typeid(float))
            labels.push_back(std::to_string(std::any_cast<float>(v)) + suffix);
        else if (v.type() == typeid(int))
            labels.push_back(std::to_string(std::any_cast<int>(v)) + suffix);
        else if (v.type() == typeid(uint32_t))
            labels.push_back(std::to_string(std::any_cast<uint32_t>(v)) + suffix);
        else if (v.type() == typeid(bool))
            labels.push_back((std::any_cast<bool>(v) ? "On" : "Off") + suffix);
        else
            assert(false); // needs more string conversion
    }
}

void MenuItem::initSelection()
{
    int oldValueIdx = valueIdx;
    valueIdx = -1;
    if (!values.empty())
    {
        valueIdx = 0;
        if (on_get)
        {
            // we know we can convert both std::any values to the same type
            const auto initialVal = on_get();
            try
            {
                for (int i = 0; i < values.size(); i++)
                {
                    const auto &v = values[i];
                    if (v.type() != initialVal.type())
                        LOG_error("type mismatch: %s vs. %s", v.type().name(), initialVal.type().name());

                    assert(v.type() == initialVal.type());

                    if (v.type() == typeid(float))
                    {
                        if (std::any_cast<float>(initialVal) == std::any_cast<float>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(int))
                    {
                        if (std::any_cast<int>(initialVal) == std::any_cast<int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(unsigned int))
                    {
                        if (std::any_cast<unsigned int>(initialVal) == std::any_cast<unsigned int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(uint32_t))
                    {
                        if (std::any_cast<uint32_t>(initialVal) == std::any_cast<uint32_t>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(bool))
                    {
                        if (std::any_cast<bool>(initialVal) == std::any_cast<bool>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::string))
                    {
                        if (std::any_cast<std::string>(initialVal) == std::any_cast<std::string>(v))
                        {
                            //LOG_info("Found equal strings %s and %s\n", initialVal, v);
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::basic_string<char>))
                    {
                        if (std::any_cast<std::basic_string<char>>(initialVal) == std::any_cast<std::basic_string<char>>(v))
                        {
                            //LOG_info("Found equal basic strings %s and %s\n", initialVal, v);
                            valueIdx = i;
                            break;
                        }
                    }
                    else
                    {
                        LOG_warn("Cant initialize selection for %s from unknown type %s\n", this->getLabel(), v.type().name());
                        // assert(false);
                    }
                }
            }
            catch (...)
            {
                // bad any cast
                LOG_warn("Bad any cast for %s\n", this->getLabel());
            }
            // this sadly doesnt work with std::any
            // auto it = std::find(values.cbegin(), values.cend(), on_get());
            // if (it == std::end(values))
            //    valueIdx = -1;
            // else
            //    valueIdx = std::distance(values.cbegin(), it);
        }
        assert(valueIdx >= 0);
    }
}

InputReactionHint MenuItem::handleInput(int &dirty)
{
    InputReactionHint hint = Unhandled;

    if (deferred)
    {
        assert(submenu);
        int subMenuJustClosed = 0;
        hint = submenu->handleInput(dirty, subMenuJustClosed);
        if (subMenuJustClosed) {
            defer(false);
            dirty = 1;
        }
        return hint;
    }

    // handle our custom behavior and return true if the input was handled
    if (PAD_justRepeated(BTN_LEFT))
    {
        hint = NoOp;
        if (prevValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_RIGHT))
    {
        hint = NoOp;
        if (nextValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    if (PAD_justRepeated(BTN_L1))
    {
        hint = NoOp;
        if (prev(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_R1))
    {
        hint = NoOp;
        if (next(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justPressed(BTN_A))
    {
        hint = NoOp; // not really, should check on_confirm
        if (on_confirm)
            hint = on_confirm(*this);
        dirty = 1;
    }

    return hint;
}

bool MenuItem::nextValue()
{
    return next(1);
}

bool MenuItem::prevValue()
{
    return prev(1);
}

bool MenuItem::next(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + n) % values.size();
    return true;
}

bool MenuItem::prev(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + values.size() - n) % values.size();
    return true;
}

///////////////////////////////////////////////////////////

MenuList::MenuList(MenuItemType type, const std::string &descp, std::vector<MenuItem*> items, MenuListCallback on_change, MenuListCallback on_confirm)
    : type(type), desc(descp), items(items), on_change(on_change), on_confirm(on_confirm)
{
    // best effort layout based on the platform defines, user should really call performLayout manually
    performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
    layout_called = false;
}

MenuList::~MenuList()
{
    WriteLock w(itemLock);
    for (auto item : items)
        delete item;
    items.clear();
}

void MenuList::performLayout(const SDL_Rect &dst)
{
    ReadLock r(itemLock);
    // TODO: consecutive calls to this should only update max_visible rows
    // and try to persist the current selection state
    // TODO: If we ever need to add menu entries dynamically, this potentially
    // needs to be called again after.
    scope.start = 0;
    scope.selected = 0;
    scope.count = items.size();
    if (type == MenuItemType::Main)
    {
        scope.max_visible_options = (dst.h - SCALE1(PILL_SIZE)) / SCALE1(PILL_SIZE);
    }
    else
    {
        // we are leaving some space to show the description label here, account for roughly two lines or one pill
        // also account for the scroll icon, in case we need it.
        // scope.max_visible_options = (dst.h - SCALE1(PILL_SIZE * 2)) / SCALE1(BUTTON_SIZE);
        scope.max_visible_options = (dst.h - SCALE1(PILL_SIZE)) / SCALE1(BUTTON_SIZE);
    }
    scope.end = std::min(scope.count, scope.max_visible_options);
    scope.visible_rows = scope.end;

    for (auto itm : items)
        if (itm->getSubMenu())
            itm->getSubMenu()->performLayout(dst);

    layout_called = true;
}

bool MenuList::selectNext()
{
    scope.selected++;
    if (scope.selected >= scope.count)
    {
        scope.selected = 0;
        scope.start = 0;
        scope.end = scope.visible_rows;
    }
    else if (scope.selected >= scope.end)
    {
        scope.start++;
        scope.end++;
    }

    return true;
}

bool MenuList::selectPrev()
{
    scope.selected--;
    if (scope.selected < 0)
    {
        scope.selected = scope.count - 1;
        scope.start = std::max(0, scope.count - scope.max_visible_options);
        scope.end = scope.count;
    }
    else if (scope.selected < scope.start)
    {
        scope.start--;
        scope.end--;
    }

    return true;
}

// returns true if the input was handled
InputReactionHint MenuList::handleInput(int &dirty, int &quit)
{
    ReadLock r(itemLock);
    InputReactionHint handled = items.at(scope.selected)->handleInput(dirty);
    if(handled == ResetAllItems) {
        resetAllItems();
        dirty = 1;
        return NoOp;
    }
    else if (handled == Exit) {
        quit = 1;
        return NoOp;
    }
    else if (handled != Unhandled)
        return handled;

    if (PAD_justRepeated(BTN_UP))
    {
        if (scope.selected == 0 && !PAD_justPressed(BTN_UP))
            return NoOp; // stop at top
        if (selectPrev())
            dirty = 1;
        return NoOp;
    }
    else if (PAD_justRepeated(BTN_DOWN))
    {
        if (scope.selected == scope.count - 1 && !PAD_justPressed(BTN_DOWN))
            return NoOp; // stop at bottom
        if (selectNext())
            dirty = 1;
        return NoOp;
    }
    else if (on_change)
    {
        // do we even need this?
    }
    else if (on_confirm && PAD_justPressed(BTN_A))
    {
        // do we even need this?
    }
    else if (PAD_justPressed(BTN_B))
    {
        quit = 1;
        return NoOp;
    }

    return Unhandled;
}

SDL_Rect MenuList::itemSizeHint(const MenuItem &item)
{
    if (type == MenuItemType::Fixed)
    {
        return {0, 0, 0, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::List)
    {
        // calculate the size of the list
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Input || type == MenuItemType::Var)
    {
        int w = 0;
        int lw = 0;
        int rw = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &lw, NULL);
        // get the width of the widest row
        int mrw = 0;
        // every value list in an input table is the same
        // so only calculate rw for the first item...
        if (!mrw || type != MenuItemType::Input)
        {
            for (int j = 0; item.getValues().size() > j && !item.getLabels()[j].empty(); j++)
            {
                TTF_SizeUTF8(font.tiny, item.getLabels()[j].c_str(), &rw, NULL);
                if (lw + rw > w)
                    w = lw + rw;
                if (rw > mrw)
                    mrw = rw;
            }
        }
        else
        {
            w = lw + mrw;
        }
        w += SCALE1(OPTION_PADDING * 4);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Main)
    {
        int w = 0;
        TTF_SizeUTF8(font.large, item.getName().c_str(), &w, NULL);
        w += SCALE1(BUTTON_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else
    {
        assert(false); // new enum value?
    }
}

void MenuList::draw(SDL_Surface *surface, const SDL_Rect &dst)
{
    assert(layout_called);
    ReadLock r(itemLock);

    auto cur = !items.empty() ? items.at(scope.selected) : nullptr;
    if (cur && cur->isDeferred())
    {
        assert(cur->getSubMenu());
        cur->getSubMenu()->draw(surface, dst);
    }
    else
    {
        // iterate all items and draw them vertically
        switch (type)
        {
        case MenuItemType::List:
            drawList(surface, dst);
            break;
        case MenuItemType::Fixed:
            drawFixed(surface, dst);
            break;
        case MenuItemType::Var:
        case MenuItemType::Input:
            drawInput(surface, dst);
            break;
        case MenuItemType::Main:
            drawMain(surface, dst);
            break;
        case MenuItemType::Custom:
            drawCustom(surface, dst);
            return; // no further drawing over custom
        default:
            assert(false && "Unknown list type");
        }

        // Handle overflow (anything but Main and Custom)
        if (type != MenuItemType::Main && items.size() > scope.max_visible_options)
        {
            const int SCROLL_WIDTH = 24;
            const int SCROLL_HEIGHT = 4;
            SDL_Rect rect = dst;
            rect = dx(rect, (rect.w - SCALE1(SCROLL_WIDTH)) / 2);
            rect = dy(rect, SCALE1((-SCROLL_HEIGHT) / 2));

            if (scope.start > 0)
                // assumes there is some padding above we can yoink
                GFX_blitAssetCPP(ASSET_SCROLL_UP, {}, surface, {rect.x, rect.y - SCALE1(PADDING)});
            if (scope.end < scope.count)
                // this is with 2 * pill_size bottom margin
                // GFX_blitAssetCPP(ASSET_SCROLL_DOWN, {}, surface, {rect.x, rect.h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + rect.y});
                GFX_blitAssetCPP(ASSET_SCROLL_DOWN, {}, surface, {rect.x, rect.h - SCALE1(PADDING + PILL_SIZE) + rect.y});
        }

        if (cur && cur->getDesc().length() > 0)
        {
            int w, h;
            const auto description = cur->getDesc();
            GFX_sizeText(font.tiny, description.c_str(), SCALE1(FONT_SMALL), &w, &h);
            GFX_blitTextCPP(font.tiny, description.c_str(), SCALE1(FONT_SMALL), COLOR_WHITE, surface, {(dst.x + dst.w - w) / 2, dst.y + dst.h - h, w, h});
        }
    }
}

void MenuList::drawList(SDL_Surface *surface, const SDL_Rect &dst)
{
    // we ignore type here, it all paints the same.
    if (max_width == 0)
    {
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        // cache the result
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * BUTTON_SIZE));
        drawListItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawListItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    // int ox = (dst.w - w) / 2; // if we're centering these (but I don't think we should after seeing it)
    if (selected)
    {
        // move out of conditional if centering
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);

        GFX_blitPillDarkCPP(ASSET_BUTTON, surface, {dst.x, dst.y, w, SCALE1(BUTTON_SIZE)});
        text_color = uintToColour(THEME_COLOR5_255);
    }
    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);
}

void MenuList::drawFixed(SDL_Surface *surface, const SDL_Rect &dst)
{
    // NOTE: no need to calculate max width
    int mw = dst.w;
    // int lw,rw;
    // lw = rw = mw / 2;
    max_width = mw; // not sure about this one

    SDL_Rect rect = dst;

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * BUTTON_SIZE));
        drawFixedItem(surface, pos, *items[i], j == selected_row);
    }
}

// TODO: expose API functions that do the same
namespace
{
    static inline void rgb_unpack(uint32_t col, int *r, int *g, int *b)
    {
        *r = (col >> 16) & 0xff;
        *g = (col >> 8) & 0xff;
        *b = col & 0xff;
    }

    static inline uint32_t mapUint(SDL_Surface *surface, uint32_t col)
    {
        int r, g, b;
        rgb_unpack(col, &r, &g, &b);
        return SDL_MapRGB(surface->format, r, g, b);
    }
}

void MenuList::drawFixedItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Color text_color_value = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    // hack - this should be correlated to max_width
    int mw = dst.w;

    if (selected)
    {
        // gray pill
        GFX_blitPillLightCPP(ASSET_BUTTON, surface, {dst.x, dst.y, mw, SCALE1(BUTTON_SIZE)});
    }

    if (item.getValue().has_value())
    {
        text = TTF_RenderUTF8_Blended(font.tiny, item.getLabel().c_str(), text_color_value);

        if (item.getType() == ListItemType::Color)
        {
            uint32_t color = mapUint(surface, std::any_cast<uint32_t>(item.getValue()));
            SDL_Rect rect = {
                dst.x + dst.w - SCALE1(OPTION_PADDING + FONT_TINY),
                dst.y + SCALE1(BUTTON_SIZE - FONT_TINY) / 2,
                SCALE1(FONT_TINY), SCALE1(FONT_TINY)};
            SDL_FillRect(surface, &rect, RGB_WHITE);
            rect = dy(dx(rect, 1), 1);
            rect.h -= 1;
            rect.w -= 1;
            SDL_FillRect(surface, &rect, color);
#define COLOR_PADDING 4
            SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING + COLOR_PADDING + FONT_TINY), dst.y + SCALE1(3)});
        }
        else if(item.getType() == ListItemType::Button) {
            // dont draw anything for now, could be a button hint later
        }
        else if(item.getType() == ListItemType::Custom) {
            item.drawCustomItem(surface, dst, item, selected);
        }
        else // Generic and fallback
            SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING), dst.y + SCALE1(3)});
        SDL_FreeSurface(text);
    }

    // TODO: blit a black pill on unselected rows (to cover longer item->values?) or truncate longer item->values?
    if (selected)
    {
        // white pill
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        GFX_blitPillDarkCPP(ASSET_BUTTON, surface, {dst.x, dst.y, w, SCALE1(BUTTON_SIZE)});
        text_color = uintToColour(THEME_COLOR5_255);
    }

    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);
}

void MenuList::drawInput(SDL_Surface *surface, const SDL_Rect &dst)
{
    // TODO: handle type if we need it
    if (max_width == 0)
    {
        // get the width of the widest row
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        // cache the result
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * BUTTON_SIZE));
        pos.w = max_width;
        drawInputItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawInputItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected)
{
    SDL_Color text_color = COLOR_WHITE;
    SDL_Surface *text;

    // hack
    int mw = dst.w;

    if (selected)
    {
        // gray pill
        GFX_blitPillLightCPP(ASSET_BUTTON, surface, {dst.x, dst.y, mw, SCALE1(BUTTON_SIZE)});

        // white pill
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        GFX_blitPillDarkCPP(ASSET_BUTTON, surface, {dst.x, dst.y, w, SCALE1(BUTTON_SIZE)});
        text_color = COLOR_BLACK;
    }
    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);

    if (/*await_input &&*/ selected)
    {
        // buh
    }
    else if (item.getValue().has_value())
    {
        text = TTF_RenderUTF8_Blended(font.tiny, item.getLabel().c_str(), COLOR_WHITE); // always white
        SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
        SDL_FreeSurface(text);
    }
}

void MenuList::drawMain(SDL_Surface *surface, const SDL_Rect &dst)
{
    // we ignore type here, it all paints the same.
    // no size calc to do here, each line is as wide as needed.

    if (scope.count > 0)
    {
        int selected_row = scope.selected - scope.start;
        for (int i = scope.start, j = 0; i < scope.end; i++, j++)
        {
            auto pos = dy(dst, SCALE1(j * PILL_SIZE));
            pos.h = SCALE1(PILL_SIZE);
            // auto hintRect = itemSizeHint(items[i]);
            // pos.w = std::min(pos.w, hintRect.w);
            // pos.h = std::min(pos.h, hintRect.h);

            drawMainItem(surface, pos, *items[i], j == selected_row);
        }
    }
    else
    {
        GFX_blitMessageCPP(font.large, "Empty folder", surface, dst);
    }
}

void MenuList::drawMainItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected)
{
    SDL_Color text_color = COLOR_WHITE;
    SDL_Surface *text;

    // TODO: unique item handling (draws grey text)
    const bool unique = false;

    char truncated[256];
    int text_width = GFX_truncateText(font.large, item.getName().c_str(), truncated, dst.w, SCALE1(BUTTON_PADDING * 2));
    int max_width = std::min(dst.w, text_width);

    if (selected)
    {
        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, max_width, dst.h});
        text_color = COLOR_BLACK;
    }
    else if (unique)
    {
        // TODO: port this over when needed. Its complete spaghetti code...
    }
    text = TTF_RenderUTF8_Blended(font.large, truncated, text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(BUTTON_PADDING), dst.y + SCALE1(3)});
    SDL_FreeSurface(text);
}

void MenuList::resetAllItems()
{
    ReadLock r(itemLock);
    for(auto item : items) {
        if(item->on_reset) {
            item->on_reset();
            item->initSelection();
        }
    }
}