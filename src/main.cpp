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
            g_status->setString(enabled ? "BOT: ON" : "BOT: OFF");
            g_status->setColor(enabled ? ccc3(80, 255, 100) : ccc3(255, 100, 100));
        }
        Notification::create(enabled ? "AutoBot enabled" : "AutoBot disabled", NotificationIcon::Info)->show();
    }
}

class $modify(AutoBotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botEnabled = false;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto hud = CCNode::create();
        g_status = CCLabelBMFont::create("BOT: OFF", "goldFont.fnt");
        g_status->setScale(0.42f);
        g_status->setAnchorPoint({1.f, 0.5f});
        g_status->setPosition({-8.f, 12.f});
        hud->addChild(g_status);
        auto sprite = ButtonSprite::create("BOT ON/OFF", 92, 0, 0.55f, true, "goldFont.fnt", "GJ_button_04.png", 25.f);
        auto toggle = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AutoBotPlayLayer::onToggleBot));
        auto menu = CCMenu::create();
        menu->addChild(toggle);
        menu->setPosition({win.width - 62.f, win.height - 72.f});
        hud->addChild(menu);
        hud->setID("gd-autobot-hud"_spr);
        this->addChild(hud, 10000);
        return true;
    }

    void onToggleBot(CCObject*) { setBotEnabled(!g_botEnabled); }

    void onQuit() {
        setBotEnabled(false);
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("GD AutoBot loaded: independent toggle HUD prototype ready");
}
