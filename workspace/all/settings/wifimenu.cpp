#include "wifimenu.hpp"
#include "keyboardprompt.hpp"

#include <unordered_set>
#include <map>

#include <shared_mutex>
typedef std::shared_mutex Lock;
typedef std::unique_lock<Lock> WriteLock;
typedef std::shared_lock<Lock> ReadLock;

using namespace Wifi;
using namespace std::placeholders;

Menu::Menu(const int &globalQuit) : MenuList(MenuItemType::Fixed, "Network", {}), globalQuit(globalQuit)
{
    toggleItem = new MenuItem(ListItemType::Generic, "WiFi", "Enable/disable WiFi", {false, true}, {"Off", "On"},
                              std::bind(&Menu::getWifToggleState, this),
                              std::bind(&Menu::setWifiToggleState, this, std::placeholders::_1),
                              std::bind(&Menu::resetWifiToggleState, this));
    items.push_back(toggleItem);

    // best effort layout based on the platform defines, user should really call performLayout manually
    MenuList::performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
    layout_called = false;

    worker = std::thread{&Menu::updater, this};
}

Menu::~Menu()
{
    quit = true;
    if (worker.joinable())
        worker.join();
}

InputReactionHint Menu::handleInput(int &dirty, int &quit)
{
    auto ret = MenuList::handleInput(dirty, quit);
    if (workerDirty)
    {
        dirty = true;
        workerDirty = false; // handled
        //LOG_info("collected workerDirty\n");
    }
    return ret;
}

std::any Menu::getWifToggleState() const
{
    return WIFI_enabled();
}

void Menu::setWifiToggleState(const std::any &on)
{
    WIFI_enable(std::any_cast<bool>(on));
}

void Menu::resetWifiToggleState()
{
    //
}

template <typename Map>
bool key_compare(Map const &lhs, Map const &rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                                                  [](auto a, auto b)
                                                  { return a.first == b.first; });
}

void Menu::updater()
{
    int pollSecs = 15;
    std::map<std::string, WIFI_network> prevScan;
    std::string prevSsid;
    while (!quit && !globalQuit)
    {
        // TODO: pause when menu is not rendered
        // Scan
        if (WIFI_enabled())
        {
            // scan for available networks and add a menu item for each
            WIFI_connection connection;
            WIFI_connectionInfo(&connection);

            // grab list and compare it to previous result
            // only relayout the menu if changes happended
            std::vector<WIFI_network> scanResults(SCAN_MAX_RESULTS);
            int cnt = WIFI_scan(scanResults.data(), SCAN_MAX_RESULTS);
            if(cnt < 0)
                continue; // try again in a bit

            std::map<std::string, WIFI_network> scanSsids;
            for (int i = 0; i < cnt; i++)
                scanSsids.emplace(scanResults[i].ssid, scanResults[i]);

            // dont repopulate if any submenu is open
            bool menuOpen = false;
            for(auto i : items){
                if(i->isDeferred()){
                    menuOpen = true;
                    break;
                }
            }

            // something changed?
            if (!menuOpen &&
                (prevSsid != std::string(connection.ssid) 
                || !key_compare(prevScan, scanSsids)))
            {
                prevScan = scanSsids;
                prevSsid = connection.ssid;

                WriteLock w(itemLock);
                items.clear();
                items.push_back(toggleItem);
                layout_called = false;

                for (auto &[s, r] : scanSsids)
                {
                    bool connected = false;
                    bool hasCredentials = WIFI_isKnown(r.ssid, r.security);

                    if (strcmp(connection.ssid, r.ssid) == 0)
                        connected = true;

                    MenuList *options;
                    if (connected)
                        options = new MenuList(MenuItemType::List, "Options",
                                               {
                                                   new MenuItem{ListItemType::Button, "Disconnect", "Disconnect from this network.",
                                                                [&](MenuItem &item) -> InputReactionHint
                                                                { WIFI_disconnect(); workerDirty = true; return Exit; }},
                                                   new MenuItem{ListItemType::Button, "Forget", "Removes credentials for this network.",
                                                                [&](MenuItem &item) -> InputReactionHint
                                                                { WIFI_forget(r.ssid, r.security); workerDirty = true; return Exit; }},
                                               });
                    else if (hasCredentials)
                        options = new MenuList(MenuItemType::List, "Options",
                        {
                            new MenuItem{ListItemType::Button, "Connect", "Connect to this network.", [&](MenuItem &item) -> InputReactionHint
                                        {
                                            std::thread([&](){ WIFI_connect(r.ssid, r.security); }).detach();
                                            workerDirty = true; 
                                            return Exit;
                                        }},
                        });
                    else
                        options = new MenuList(MenuItemType::List, "Options",
                        {
                            new MenuItem{ListItemType::Button, "Enter WiFi passcode", "Connect to this network.", DeferToSubmenu, new KeyboardPrompt("Enter Wifi passcode", 
                                [&](MenuItem &item) -> InputReactionHint {
                                    std::thread([=](){ WIFI_connectPass(r.ssid, r.security, item.getName().c_str()); }).detach();
                                    workerDirty = true; 
                                    return Exit; 
                                })},
                        });

                    auto itm = new NetworkItem{r, connected, options};
                    if(connected && !std::string(connection.ip).empty())
                        itm->setDesc(std::string(r.bssid) + " | " + std::string(connection.ip));
                    items.push_back(itm);
                }
                workerDirty = true;
            }
            pollSecs = 2;
        }
        else
        {
            WriteLock w(itemLock);
            items.clear();
            items.push_back(toggleItem);
            layout_called = false;
            workerDirty = true;
            pollSecs = 15;
        }

        // reset selection scope (locks internally)
        if (workerDirty)
        {
            MenuList::performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
        }

        std::this_thread::sleep_for(std::chrono::seconds(pollSecs));
    }
}


NetworkItem::NetworkItem(WIFI_network n, bool connected, MenuList* submenu)
    : MenuItem(ListItemType::Custom, n.ssid, n.bssid, DeferToSubmenu, submenu), net(n), connected(connected)
{}

void NetworkItem::drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected) const
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text = TTF_RenderUTF8_Blended(font.tiny, item.getLabel().c_str(), COLOR_WHITE); // always white

    // hack - this should be correlated to max_width
    int mw = dst.w;

    if (selected)
    {
        // gray pill
        GFX_blitPillLightCPP(ASSET_BUTTON, surface, {dst.x, dst.y, mw, SCALE1(BUTTON_SIZE)});
    }

    // wifi icon
    auto asset =
        net.rssi > 67 ? ASSET_WIFI :
        net.rssi > 70 ? ASSET_WIFI_MED
                      : ASSET_WIFI_LOW;
    SDL_Rect rect = {0, 0, 14, 10};
    int ix = dst.x + dst.w - SCALE1(OPTION_PADDING + rect.w);
    int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
    SDL_Rect tgt{ix, y};
    GFX_blitAssetColor(asset, NULL, surface, &tgt, THEME_COLOR3);

    // connected
    if(connected) {
        SDL_Rect rect = {0, 0, 12, 12};
        ix = ix - SCALE1(OPTION_PADDING + rect.w);
        int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
        SDL_Rect tgt{ix, y};
        GFX_blitAssetColor(ASSET_CHECKCIRCLE, NULL, surface, &tgt, THEME_COLOR3);
    }
    // encrypted
    else if(net.security != SECURITY_NONE) {
        SDL_Rect rect = {0, 0, 8, 11};
        ix = ix - SCALE1(OPTION_PADDING + rect.w + 2);
        int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
        SDL_Rect tgt{ix, y};
        GFX_blitAssetColor(ASSET_LOCK, NULL, surface, &tgt, THEME_COLOR3);
    }

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