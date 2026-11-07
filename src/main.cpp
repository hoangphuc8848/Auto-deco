#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

using namespace geode::prelude;

namespace {
    bool g_botEnabled = false;
    CCLabelBMFont* g_status = nullptr;

    void setBotEnabled(bool enabled) {
        g_botEnabled = enabled;
        if (g_status) {
            g_status->setString(enabled ? "BOT ON - TEST MODE" : "BOT OFF");
            g_status->setColor(enabled ? ccc3(255, 190, 40) : ccc3(255, 100, 100));
        }
        Notification::create(
            enabled ? "Bot toggle ON (AI control not connected yet)" : "AutoBot OFF",
            NotificationIcon::Info
        )->show();
    }
}

class $modify(AutoBotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botEnabled = false;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto hud = CCNode::create();

        g_status = CCLabelBMFont::create("BOT OFF", "goldFont.fnt");
        g_status->setScale(0.38f);
        g_status->setAnchorPoint({1.f, 0.5f});
        g_status->setPosition({win.width - 12.f, win.height - 48.f});
        g_status->setColor(ccc3(255, 100, 100));
        hud->addChild(g_status);

        auto sprite = ButtonSprite::create("BOT ON/OFF", 92, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 25.f);
        auto toggle = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AutoBotPlayLayer::onToggleBot));
        auto menu = CCMenu::create();
        menu->addChild(toggle);
        menu->setPosition({win.width - 62.f, win.height - 78.f});
        hud->addChild(menu);
        hud->setID("gd-autobot-hud"_spr);
        this->addChild(hud, 10000);
        return true;
    }

    void onToggleBot(CCObject*) { setBotEnabled(!g_botEnabled); }

    void onQuit() {
        g_botEnabled = false;
        g_status = nullptr;
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("GD AutoBot loaded: toggle HUD prototype; gameplay input is not connected yet");
}
