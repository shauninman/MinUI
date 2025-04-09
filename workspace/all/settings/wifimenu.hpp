#pragma once

#include "menu.hpp"
#include <thread>

namespace Wifi
{
    class Menu : public MenuList
    {
        const int &globalQuit;
        // wifi on/off
        MenuItem *toggleItem;

        std::thread worker;
        bool quit = false;
        bool workerDirty = false;

    public:
        Menu(const int &globalQuit);
        ~Menu();

        InputReactionHint handleInput(int &dirty, int &quit) override;

    private:
        std::any getWifToggleState() const;
        void setWifiToggleState(const std::any &on);
        void resetWifiToggleState();

        void updater();
    };

    class NetworkItem : public MenuItem
    {
        WIFI_network net;
        bool connected;

    public:
        NetworkItem(WIFI_network n, bool connected, MenuList *submenu);

        void drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const MenuItem &item, bool selected) const override;
    };
}