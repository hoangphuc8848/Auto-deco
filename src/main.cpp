#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/EffectGameObject.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/MDPopup.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include "OfflineAssistant.hpp"
#include "AutoDecoEngine.hpp"
#include "GameplayBuilder.hpp"

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

    const char* segmentName(gdautobot::builder::Segment type) {
        using gdautobot::builder::Segment;
        switch (type) {
            case Segment::Run: return "Run";
            case Segment::Jump: return "Jump";
            case Segment::Orb: return "Orb";
            case Segment::Platform: return "Platform";
            case Segment::Transition: return "Transition";
            case Segment::WaveCorridor: return "Wave corridor";
        }
        return "Unknown";
    }

    ccColor3B themeColor(std::string const& theme) {
        if (theme == "hell") return ccc3(235, 55, 35);
        if (theme == "space") return ccc3(55, 190, 255);
        if (theme == "forest") return ccc3(80, 190, 95);
        if (theme == "nine-circles") return ccc3(255, 55, 145);
        return ccc3(35, 225, 235);
    }

    int placeEditorObject(LevelEditorLayer* editor, int objectID, cocos2d::CCPoint position, ccColor3B color) {
        if (!editor) return 0;
        auto object = editor->createObject(objectID, position, false);
        if (!object) return 0;
        object->setObjectColor(color);
        object->addToGroup(9000);
        return 1;
    }

    int applyDecoration(LevelEditorLayer* editor, std::string const& theme) {
        if (!editor || !editor->m_objectLayer) return 0;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto center = editor->m_objectLayer->convertToNodeSpace({win.width * 0.5f, win.height * 0.5f});
        auto color = themeColor(theme);
        int placed = 0;
        for (int i = 0; i < 8; ++i) {
            placed += placeEditorObject(editor, 1, {center.x - 112.f + i * 32.f, center.y - 80.f}, color);
            if (i % 2 == 0) placed += placeEditorObject(editor, 1, {center.x - 112.f + i * 32.f, center.y - 48.f}, ccc3(35, 35, 48));
        }
        for (int i = 0; i < 4; ++i) placed += placeEditorObject(editor, 1, {center.x - 48.f + i * 32.f, center.y - 16.f}, color);
        return placed;
    }

    int buildTriggerSet(LevelEditorLayer* editor) {
        if (!editor || !editor->m_objectLayer) return 0;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto center = editor->m_objectLayer->convertToNodeSpace({win.width * 0.5f, win.height * 0.5f});

        // Generated objects use group 9000; effect triggers target that group.
        struct TriggerSpec { int id; float x; ccColor3B color; bool hasGroupTarget; };
        const TriggerSpec triggers[] = {
            {899, -100.f, ccc3(255, 205, 70), false},  // Color
            {901, -60.f, ccc3(80, 170, 255), true},    // Move
            {1006, -20.f, ccc3(255, 80, 160), true},   // Pulse
            {1007, 20.f, ccc3(255, 190, 80), true},    // Alpha
            {1049, 60.f, ccc3(245, 80, 80), true},     // Toggle
            {1268, 100.f, ccc3(80, 220, 130), true}    // Spawn
        };

        int placed = 0;
        for (auto const& trigger : triggers) {
            auto object = editor->createObject(trigger.id, {center.x + trigger.x, center.y + 32.f}, false);
            if (!object) continue;
            object->setObjectColor(trigger.color);
            if (trigger.hasGroupTarget) {
                if (auto effect = typeinfo_cast<EffectGameObject*>(object)) {
                    effect->setTargetID(9000);
                }
            }
            ++placed;
        }
        return placed;
    }

    int buildGameplayObjects(LevelEditorLayer* editor, int difficulty) {
        if (!editor || !editor->m_objectLayer) return 0;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto center = editor->m_objectLayer->convertToNodeSpace({win.width * 0.5f, win.height * 0.5f});
        int placed = 0;
        const float startX = center.x + 80.f;
        const float floorY = center.y - 80.f;
        for (int i = 0; i < 12; ++i) placed += placeEditorObject(editor, 1, {startX + i * 30.f, floorY}, ccc3(80, 145, 220));
        const int spikeEvery = difficulty >= 8 ? 2 : 3;
        for (int i = 2; i < 11; i += spikeEvery) placed += placeEditorObject(editor, 8, {startX + i * 30.f, floorY + 30.f}, ccc3(245, 90, 90));
        if (difficulty >= 7) for (int i = 0; i < 3; ++i) placed += placeEditorObject(editor, 1, {startX + 240.f + i * 30.f, floorY + 30.f}, ccc3(80, 145, 220));
        return placed;
    }
    std::string makeAssistantAnswer(std::string const& prompt) {
        using namespace gdautobot;
        auto request = offline::parse(prompt);
        std::ostringstream out;
        out << "**Offline Builder result**\n\n";
        out << "_Local rule-based assistant. No API, network or model download._\n\n";

        if (request.intent == offline::Intent::Help || request.intent == offline::Intent::Unknown) {
            out << "**Commands**\n"
                << "- deco hell / deco tech / deco space / deco forest / deco nine circles\n"
                << "- palette tech (or another theme)\n"
                << "- gameplay easy / gameplay hard\n"
                << "- Add glow or wave to refine a request\n\n"
                << "Example: deco nine circles glow";
            return out.str();
        }

        std::string theme = request.theme.empty() ? "tech" : request.theme;
        if (request.intent == offline::Intent::Decorate ||
            request.intent == offline::Intent::SuggestPalette) {
            auto plan = deco::makePlan(theme, request.glow);
            out << "**Theme:** " << plan.theme << "\n\n";
            out << "**Suggested color IDs:** primary " << plan.colors.primary
                << ", secondary " << plan.colors.secondary
                << ", accent " << plan.colors.accent << "\n\n";
            out << "**Decoration layers**\n";
            for (auto const& layer : plan.layers) out << "- " << layer << "\n";
            out << "\n**Placement advice:** build the background first, keep the player path readable, "
                   "then add edge details and sparse effects.";
            return out.str();
        }

        if (request.intent == offline::Intent::BuildGameplay) {
            bool wave = prompt.find("wave") != std::string::npos ||
                        prompt.find("Wave") != std::string::npos;
            auto section = builder::generate(request.difficulty, wave, 900.f);
            out << "**Gameplay section**\n\n";
            out << "Difficulty intensity: " << section.difficulty << "/10  \n";
            out << "Target length: " << static_cast<int>(section.totalLength) << " units\n\n";
            out << "| Part | Length | Intensity | Purpose |\n|---|---:|---:|---|\n";
            for (auto const& piece : section.pieces) {
                out << "| " << segmentName(piece.type) << " | "
                    << static_cast<int>(piece.length) << " | "
                    << piece.intensity << " | " << piece.note << " |\n";
            }
            out << "\nUse this as a layout blueprint; it does not place objects automatically yet.";
            return out.str();
        }
        return "I couldn't map that request. Try help for supported commands.";
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
            if (!obj || static_cast<int>(obj->m_objectType) != 2) continue;
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

class $modify(OfflineAssistantEditorUI, EditorUI) {
    struct Fields {
        geode::TextInput* prompt = nullptr;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto win = CCDirector::sharedDirector()->getWinSize();
        auto panel = CCNode::create();
        panel->setID("offline-builder-panel"_spr);

        auto title = CCLabelBMFont::create("OFFLINE BUILDER", "goldFont.fnt");
        title->setScale(0.42f);
        title->setPosition({0.f, 42.f});
        panel->addChild(title);

        auto input = geode::TextInput::create(220.f, "deco hell / gameplay hard");
        input->setCommonFilter(geode::CommonFilter::Any);
        input->setMaxCharCount(100);
        input->setTextAlign(geode::TextInputAlign::Left);
        input->setPosition({-35.f, 0.f});
        panel->addChild(input);
        m_fields->prompt = input;

        auto buttonSprite = ButtonSprite::create("GENERATE", 76, 0, 0.48f, true, "goldFont.fnt", "GJ_button_01.png", 20.f);
        auto button = CCMenuItemSpriteExtra::create(buttonSprite, this, menu_selector(OfflineAssistantEditorUI::onGenerate));
        auto menu = CCMenu::create();
        menu->addChild(button);
        menu->setPosition({115.f, 0.f});
        panel->addChild(menu);

        auto decoSprite = ButtonSprite::create("APPLY DECO", 88, 0, 0.42f, true, "goldFont.fnt", "GJ_button_02.png", 20.f);
        auto decoButton = CCMenuItemSpriteExtra::create(decoSprite, this, menu_selector(OfflineAssistantEditorUI::onApplyDeco));
        auto buildSprite = ButtonSprite::create("BUILD", 64, 0, 0.42f, true, "goldFont.fnt", "GJ_button_03.png", 20.f);
        auto buildButton = CCMenuItemSpriteExtra::create(buildSprite, this, menu_selector(OfflineAssistantEditorUI::onBuildGameplay));
        auto triggerSprite = ButtonSprite::create("TRIGGERS", 76, 0, 0.42f, true, "goldFont.fnt", "GJ_button_04.png", 20.f);
        auto triggerButton = CCMenuItemSpriteExtra::create(triggerSprite, this, menu_selector(OfflineAssistantEditorUI::onBuildTriggers));
        auto actionMenu = CCMenu::create();
        actionMenu->addChild(decoButton);
        actionMenu->addChild(buildButton);
        actionMenu->addChild(triggerButton);
        decoButton->setPosition({-76.f, 0.f});
        buildButton->setPosition({0.f, 0.f});
        triggerButton->setPosition({76.f, 0.f});
        actionMenu->setPosition({35.f, -38.f});
        panel->addChild(actionMenu);

        panel->setPosition({win.width - 155.f, 82.f});
        this->addChild(panel, 10000);
        return true;
    }

    void onApplyDeco(CCObject*) {
        if (!m_editorLayer) return;
        std::string prompt = m_fields->prompt ? m_fields->prompt->getString() : "";
        auto request = gdautobot::offline::parse(prompt);
        auto theme = request.theme.empty() ? std::string("tech") : request.theme;
        int count = applyDecoration(m_editorLayer, theme);
        Notification::create(fmt::format("Offline Builder: placed {} accent blocks", count), NotificationIcon::Info)->show();
    }

    void onBuildGameplay(CCObject*) {
        if (!m_editorLayer) return;
        std::string prompt = m_fields->prompt ? m_fields->prompt->getString() : "";
        auto request = gdautobot::offline::parse(prompt);
        int count = buildGameplayObjects(m_editorLayer, request.difficulty);
        Notification::create(fmt::format("Offline Builder: placed {} gameplay objects", count), NotificationIcon::Info)->show();
    }

    void onBuildTriggers(CCObject*) {
        if (!m_editorLayer) return;
        int count = buildTriggerSet(m_editorLayer);
        Notification::create(fmt::format("Auto Special: placed {} triggers; target group 9000 assigned. Set Color channel in Edit Object.", count), NotificationIcon::Info)->show();
    }

    void onGenerate(CCObject*) {
        if (!m_fields->prompt) return;
        auto prompt = m_fields->prompt->getString();
        if (prompt.empty()) prompt = "help";
        auto answer = makeAssistantAnswer(prompt);
        geode::MDPopup::create("Offline Builder", answer, "Close")->show();
    }
};

$on_mod(Loaded) {
    log::info("GD AutoBot loaded: offline editor assistant + experimental Cube controller");
}
