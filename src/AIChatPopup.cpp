#include "AIChatPopup.hpp"
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
#include <string>

using namespace geode::prelude;

namespace {
    std::string lowerText(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    float clampToBounds(float value, float low, float high) {
        return std::max(low, std::min(value, high));
    }

    void addObject(auto_deco::Plan& plan, int id, float x, float y, float scale = 1.f, float rotation = 0.f) {
        auto_deco::PlannedObject item;
        item.kind = auto_deco::PlannedObject::Kind::Object;
        item.id = id;
        item.x = x;
        item.y = y;
        item.scale = scale;
        item.rotation = rotation;
        plan.items.push_back(item);
    }

    // A deterministic, fully local layout generator. It uses the prompt as
    // style/intent hints and never sends level data or an API key anywhere.
    auto_deco::Plan makeOfflinePlan(std::string const& rawPrompt, float minX, float maxX, float minY, float maxY) {
        auto prompt = lowerText(rawPrompt);
        auto_deco::Plan plan;
        plan.message = "Generated locally from your description.";

        const float width = std::max(120.f, maxX - minX);
        const float height = std::max(100.f, maxY - minY);
        const float left = minX + width * 0.08f;
        const float right = maxX - width * 0.08f;
        const float bottom = minY + height * 0.12f;
        const float top = maxY - height * 0.12f;
        const float centerY = (minY + maxY) * 0.5f;

        bool gameplay = prompt.find("gameplay") != std::string::npos ||
            prompt.find("buff") != std::string::npos || prompt.find("spike") != std::string::npos ||
            prompt.find("obstacle") != std::string::npos || prompt.find("demon") != std::string::npos;
        bool nature = prompt.find("forest") != std::string::npos || prompt.find("nature") != std::string::npos ||
            prompt.find("rung") != std::string::npos || prompt.find("cay") != std::string::npos;
        bool castle = prompt.find("castle") != std::string::npos || prompt.find("medieval") != std::string::npos ||
            prompt.find("thanh") != std::string::npos || prompt.find("lâu") != std::string::npos;
        bool glow = prompt.find("glow") != std::string::npos || prompt.find("neon") != std::string::npos ||
            prompt.find("sang") != std::string::npos || prompt.find("phat") != std::string::npos;
        bool dense = prompt.find("dense") != std::string::npos || prompt.find("chi tiet") != std::string::npos ||
            prompt.find("detail") != std::string::npos || prompt.find("nhiều") != std::string::npos;
        int baseID = nature ? 4 : (castle ? 6 : (glow ? 2 : 1));
        int accentID = nature ? 5 : (castle ? 7 : (glow ? 3 : 4));
        int columns = dense ? 22 : 14;
        float step = (right - left) / static_cast<float>(std::max(1, columns - 1));
        unsigned seed = static_cast<unsigned>(std::hash<std::string>{}(prompt));

        // Upper and lower framing: alternating blocks create a coherent border.
        for (int i = 0; i < columns && plan.items.size() < 150; ++i) {
            float x = left + step * i;
            float jitter = static_cast<float>((seed >> (i % 24)) & 7u) * 1.2f;
            addObject(plan, (i % 3 == 0) ? accentID : baseID, x, top - jitter, 0.65f);
            addObject(plan, (i % 3 == 1) ? accentID : baseID, x, bottom + jitter, 0.65f);
        }

        // Side pillars and a restrained center motif.
        int pillars = dense ? 7 : 5;
        for (int side = 0; side < 2; ++side) {
            float x = side == 0 ? left : right;
            for (int j = 1; j <= pillars; ++j) {
                float y = bottom + (top - bottom) * j / static_cast<float>(pillars + 1);
                addObject(plan, j % 2 ? baseID : accentID, x, y, 0.7f);
            }
        }

        float motifY = clampToBounds(centerY, minY + 30.f, maxY - 30.f);
        for (int j = -2; j <= 2; ++j) {
            addObject(plan, (j % 2 == 0) ? accentID : baseID,
                clampToBounds((minX + maxX) * 0.5f + j * 28.f, minX, maxX),
                clampToBounds(motifY + std::abs(j) * 20.f, minY, maxY), 0.8f);
        }

        // Add a few gameplay hazards only when the prompt explicitly asks.
        if (gameplay) {
            int hazardCount = dense ? 9 : 5;
            for (int i = 0; i < hazardCount; ++i) {
                float x = left + (right - left) * (i + 1) / static_cast<float>(hazardCount + 1);
                float y = clampToBounds(centerY + ((i % 2) ? 34.f : -34.f), minY + 20.f, maxY - 20.f);
                addObject(plan, 8, x, y, 0.8f, 0.f);
            }
        }

        if (plan.items.size() > 200) plan.items.resize(200);
        return plan;
    }
}

AIChatPopup* AIChatPopup::create(LevelEditorLayer* editor, float minX, float maxX, float minY, float maxY) {
    auto ret = new AIChatPopup();
    ret->m_editorLayer = editor;
    ret->m_minX = minX; ret->m_maxX = maxX;
    ret->m_minY = minY; ret->m_maxY = maxY;
    if (ret->init()) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

bool AIChatPopup::init() {
    if (!Popup::init(360.f, 265.f, "GJ_square01.png")) return false;
    this->setTitle("Auto Deco - Offline AI");

    auto info = CCLabelBMFont::create("Runs locally - no API key / Internet / credits", "goldFont.fnt");
    info->setScale(0.38f); info->setPosition({180.f, 218.f}); m_mainLayer->addChild(info);

    auto promptLabel = CCLabelBMFont::create("Describe the style or gameplay to generate", "goldFont.fnt");
    promptLabel->setScale(0.38f); promptLabel->setAnchorPoint({0.f, 0.5f});
    promptLabel->setPosition({22.f, 175.f}); m_mainLayer->addChild(promptLabel);

    m_promptInput = TextInput::create(310.f, "Example: neon glow deco, forest, buff gameplay", "chatFont.fnt");
    m_promptInput->setPosition({180.f, 148.f});
    m_promptInput->setCommonFilter(CommonFilter::Any);
    m_promptInput->setMaxCharCount(500);
    m_mainLayer->addChild(m_promptInput);

    m_status = CCLabelBMFont::create("Describe a style, then tap GENERATE.", "chatFont.fnt");
    m_status->setScale(0.42f); m_status->setPosition({180.f, 103.f});
    m_status->setWidth(320.f); m_mainLayer->addChild(m_status);

    auto menu = CCMenu::create(); menu->setPosition({0.f, 0.f});
    auto generate = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("GENERATE", 100, 0, 0.58f, true, "goldFont.fnt", "GJ_button_02.png", 28.f),
        this, menu_selector(AIChatPopup::onSend));
    generate->setPosition({95.f, 42.f}); menu->addChild(generate);
    auto apply = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("APPLY", 100, 0, 0.58f, true, "goldFont.fnt", "GJ_button_03.png", 28.f),
        this, menu_selector(AIChatPopup::onApply));
    apply->setPosition({265.f, 42.f}); menu->addChild(apply);
    m_mainLayer->addChild(menu);
    return true;
}

void AIChatPopup::onSaveKey(CCObject*) {
    // Kept for compatibility with the existing class declaration; offline mode needs no key.
    m_status->setString("Offline mode does not use an API key.");
}

void AIChatPopup::onSend(CCObject*) {
    std::string prompt = m_promptInput->getString().c_str();
    if (prompt.empty()) {
        m_status->setString("Describe a style or gameplay first.");
        return;
    }

    m_plan.clear();
    m_validatedPlan = makeOfflinePlan(prompt, m_minX, m_maxX, m_minY, m_maxY);
    for (auto const& item : m_validatedPlan.items) {
        if (item.kind != auto_deco::PlannedObject::Kind::Object) continue;
        Placement p;
        p.id = item.id; p.x = item.x; p.y = item.y;
        p.scale = item.scale; p.rotation = item.rotation;
        m_plan.push_back(p);
    }

    m_status->setString(("Offline plan ready: " + std::to_string(m_plan.size()) +
        " objects. Review it, then tap APPLY.").c_str());
}

void AIChatPopup::onApply(CCObject*) {
    if (!m_editorLayer) { m_status->setString("Editor is no longer available."); return; }
    if (m_validatedPlan.items.empty()) { m_status->setString("Tap GENERATE first."); return; }

    int made = 0;
    for (auto const& item : m_validatedPlan.items) {
        if (item.kind != auto_deco::PlannedObject::Kind::Object) continue;
        if (item.x < m_minX || item.x > m_maxX || item.y < m_minY || item.y > m_maxY) continue;
        auto object = m_editorLayer->createObject(item.id, {item.x, item.y}, false);
        if (!object) continue;
        object->setScale(item.scale);
        object->setRotation(item.rotation);
        ++made;
    }

    m_plan.clear();
    m_validatedPlan = auto_deco::Plan{};
    m_status->setString(("Applied " + std::to_string(made) +
        " offline-generated objects. Use Undo to revert.").c_str());
}
