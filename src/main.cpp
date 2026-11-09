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
#include "BotController.hpp"
#include "LearningMemory.hpp"

using namespace geode::prelude;

namespace {
    bool g_botEnabled = false;
    bool g_jumpHeld = false;
    float g_jumpPulse = 0.f;
    float g_cooldown = 0.f;
    GameObject* g_lastHazard = nullptr;
    CCLabelBMFont* g_status = nullptr;
    gdautobot::LearningMemory g_memory;
    gdautobot::Observation g_lastObservation;
    bool g_deathHandled = false;

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
            g_status->setString(enabled ? "BOT ON - ADAPTIVE TEST" : "BOT OFF");
            g_status->setColor(enabled ? ccc3(255, 190, 40) : ccc3(255, 100, 100));
        }
        Notification::create(
            enabled ? "AutoBot ON (adaptive controller test)" : "AutoBot OFF",
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
        if (!g_botEnabled || !m_player1 || m_isPaused) {
            if (g_jumpHeld) releaseJump(m_player1);
            return;
        }

        // Observe death once, persist a failure sample, then retry the current run.
        if (m_player1->m_isDead) {
            if (!g_deathHandled) {
                g_deathHandled = true;
                releaseJump(m_player1);
                auto key = gdautobot::BotController::situationKey(g_lastObservation);
                gdautobot::FailureSample sample;
                sample.playerX = g_lastObservation.playerX;
                sample.playerY = g_lastObservation.playerY;
                sample.speed = g_lastObservation.horizontalSpeed;
                sample.action = static_cast<uint8_t>(gdautobot::Action::Press);
                g_memory.recordFailure(key, sample);
                g_lastHazard = nullptr;
                g_cooldown = 0.f;
                PlayLayer::resetLevel();
                return;
            }
            return;
        }
        g_deathHandled = false;

        auto* player = m_player1;
        gdautobot::Observation obs;
        obs.playerX = player->getPositionX();
        obs.playerY = player->getPositionY();
        obs.horizontalSpeed = std::max(1.f, player->m_playerSpeed);
        obs.grounded = player->m_isOnGround;
        if (player->m_isShip) obs.mode = gdautobot::Mode::Ship;
        else if (player->m_isDart) obs.mode = gdautobot::Mode::Wave;
        else if (player->m_isBird) obs.mode = gdautobot::Mode::UFO;
        else if (player->m_isBall) obs.mode = gdautobot::Mode::Ball;
        else if (player->m_isRobot) obs.mode = gdautobot::Mode::Robot;
        else if (player->m_isSpider) obs.mode = gdautobot::Mode::Spider;
        else if (player->m_isSwing) obs.mode = gdautobot::Mode::Swing;
        else obs.mode = gdautobot::Mode::Cube;

        g_cooldown = std::max(0.f, g_cooldown - dt);
        if (g_jumpHeld) {
            g_jumpPulse -= dt;
            if (g_jumpPulse <= 0.f) releaseJump(player);
        }

        auto objects = m_objects;
        if (!objects) return;
        GameObject* nearest = nullptr;
        float nearestDistance = 1e9f;
        float hazardY = obs.playerY;
        for (unsigned int i = 0; i < objects->count(); ++i) {
            auto obj = typeinfo_cast<GameObject*>(objects->objectAtIndex(i));
            if (!obj || static_cast<int>(obj->m_objectType) != 2) continue;
            auto rect = obj->getObjectRect();
            const float left = rect.origin.x;
            const float right = left + rect.size.width;
            const float distance = left - obs.playerX;
            if (right <= obs.playerX || distance < -8.f || distance > 190.f) continue;
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = obj;
                hazardY = rect.origin.y + rect.size.height * 0.5f;
            }
        }

        obs.hazardVisible = nearest != nullptr;
        obs.nearestHazardX = obs.playerX + nearestDistance;
        obs.nearestHazardY = hazardY;
        auto key = gdautobot::BotController::situationKey(obs);
        auto learned = g_memory.find(key);
        float learnedBias = learned ? std::min(22.f, static_cast<float>(learned->failures) * 2.f) : 0.f;
        float reaction = std::clamp(obs.horizontalSpeed * 0.22f + learnedBias, 24.f, 78.f);

        // Modes with continuous vertical control use the visible playfield as a
        // coarse corridor. Discrete modes use obstacle-triggered short taps.
        auto win = CCDirector::sharedDirector()->getWinSize();
        bool continuous = obs.mode == gdautobot::Mode::Ship ||
                          obs.mode == gdautobot::Mode::Wave ||
                          obs.mode == gdautobot::Mode::Swing;
        if (continuous) {
            float low = win.height * 0.30f;
            float high = win.height * 0.70f;
            bool shouldHold = player->getPositionY() < low;
            bool shouldRelease = player->getPositionY() > high;
            if (shouldHold && !g_jumpHeld) {
                player->pushButton(PlayerButton::Jump);
                g_jumpHeld = true;
                g_jumpPulse = 0.20f;
            } else if (shouldRelease && g_jumpHeld) {
                releaseJump(player);
            }
        } else {
            float modeReaction = reaction;
            if (obs.mode == gdautobot::Mode::UFO) modeReaction *= 1.05f;
            if (obs.mode == gdautobot::Mode::Ball) modeReaction *= 0.95f;
            if (obs.mode == gdautobot::Mode::Spider) modeReaction *= 1.15f;
            if (obs.mode == gdautobot::Mode::Robot) modeReaction *= 1.10f;
            if (nearest && nearest != g_lastHazard && nearestDistance <= modeReaction &&
                nearestDistance >= -8.f && g_cooldown <= 0.f && !g_jumpHeld) {
                player->pushButton(PlayerButton::Jump);
                g_jumpHeld = true;
                g_jumpPulse = obs.mode == gdautobot::Mode::Robot ? 0.12f : 0.075f;
                g_cooldown = obs.mode == gdautobot::Mode::UFO ? 0.20f : 0.28f;
                g_lastHazard = nearest;
            }
        }
        if (!nearest || nearestDistance > 105.f || nearestDistance < -8.f)
            g_lastHazard = nullptr;
        g_lastObservation = obs;
    }

    void resetLevel() {
        releaseJump(m_player1);
        g_lastHazard = nullptr;
        g_cooldown = 0.f;
        g_deathHandled = false;
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
    log::info("GD AutoBot loaded: offline editor assistant + adaptive multi-mode controller");
}
