#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    bool g_botEnabled = false;
    bool g_jumpHeld = false;
    float g_jumpPulse = 0.f;
    float g_cooldown = 0.f;
    GameObject* g_lastHazard = nullptr;
    CCLabelBMFont* g_status = nullptr;

    void releaseJump(PlayerObject* player) {
        if (g_jumpHeld && player) player->releaseButton(PlayerButton::Jump);
        g_jumpHeld = false;
        g_jumpPulse = 0.f;
    }

    void setBotEnabled(bool enabled, PlayerObject* player) {
        g_botEnabled = enabled;
        if (!enabled) {
            releaseJump(player);
            g_lastHazard = nullptr;
            g_cooldown = 0.f;
        }
        if (g_status) {
            g_status->setString(enabled ? "BOT ON - CUBE TEST" : "BOT OFF");
            g_status->setColor(enabled ? ccc3(255, 190, 40) : ccc3(255, 100, 100));
        }
        Notification::create(
            enabled ? "AutoBot ON (experimental Cube controller)" : "AutoBot OFF",
            NotificationIcon::Info
        )->show();
    }
}

class $modify(AutoBotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botEnabled = false;
        g_jumpHeld = false;
        g_jumpPulse = 0.f;
        g_cooldown = 0.f;
        g_lastHazard = nullptr;

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

    void onToggleBot(CCObject*) { setBotEnabled(!g_botEnabled, m_player1); }

    void update(float dt) {
        PlayLayer::update(dt);
        if (!g_botEnabled || !m_player1 || m_player1->m_isDead || m_isPaused) {
            if (g_jumpHeld) releaseJump(m_player1);
            return;
        }

        // First playable slice: standard Cube only. Other gamemodes intentionally
        // remain passive until dedicated controllers are implemented.
        if (m_player1->m_isShip || m_player1->m_isBird || m_player1->m_isBall ||
            m_player1->m_isDart || m_player1->m_isRobot || m_player1->m_isSpider ||
            m_player1->m_isSwing) {
            releaseJump(m_player1);
            return;
        }

        g_cooldown = std::max(0.f, g_cooldown - dt);
        if (g_jumpHeld) {
            g_jumpPulse -= dt;
            if (g_jumpPulse <= 0.f) releaseJump(m_player1);
        }

        auto objects = m_objects;
        if (!objects) return;
        const float px = m_player1->getPositionX();
        const float speed = std::max(1.f, m_player1->m_playerSpeed);
        GameObject* nearest = nullptr;
        float nearestDistance = 1e9f;

        for (unsigned int i = 0; i < objects->count(); ++i) {
            auto obj = typeinfo_cast<GameObject*>(objects->objectAtIndex(i));
            if (!obj || static_cast<int>(obj->m_objectType) != 2) continue; // Hazard
            auto rect = obj->getObjectRect();
            const float left = rect.origin.x;
            const float right = left + rect.size.width;
            const float distance = left - px;
            if (right <= px || distance < -8.f || distance > 150.f) continue;
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = obj;
            }
        }

        // Simple conservative Cube heuristic: tap jump once as a hazard enters
        // a speed-scaled reaction window. It is not a full trajectory solver.
        const float reactionDistance = std::clamp(speed * 0.22f, 24.f, 58.f);
        if (nearest && nearest != g_lastHazard && nearestDistance <= reactionDistance &&
            nearestDistance >= -8.f && g_cooldown <= 0.f && !g_jumpHeld) {
            m_player1->pushButton(PlayerButton::Jump);
            g_jumpHeld = true;
            g_jumpPulse = 0.075f;
            g_cooldown = 0.28f;
            g_lastHazard = nearest;
        }
        if (!nearest || nearestDistance > 95.f || nearestDistance < -8.f)
            g_lastHazard = nullptr;
    }

    void resetLevel() {
        releaseJump(m_player1);
        g_lastHazard = nullptr;
        g_cooldown = 0.f;
        PlayLayer::resetLevel();
    }

    void onQuit() {
        releaseJump(m_player1);
        g_botEnabled = false;
        g_status = nullptr;
        g_lastHazard = nullptr;
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("GD AutoBot loaded: experimental Cube hazard-reactive controller");
}
