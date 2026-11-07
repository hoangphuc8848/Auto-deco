#include "AIChatPopup.hpp"
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

using namespace geode::prelude;

namespace {
    std::string extractResponseText(std::string const& body) {
        // Responses API JSON contains a plain "text" key. Match the actual
        // JSON key (not an escaped quote sequence), then decode JSON escapes.
        size_t key = 0;
        while ((key = body.find("\"text\"", key)) != std::string::npos) {
            size_t pos = key + 6;
            while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
            if (pos >= body.size() || body[pos++] != ':') { key += 6; continue; }
            while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
            if (pos >= body.size() || body[pos++] != '"') { key += 6; continue; }

            std::string out;
            bool escaped = false;
            for (size_t i = pos; i < body.size(); ++i) {
                char c = body[i];
                if (escaped) {
                    switch (c) {
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case '"': out += '"'; break;
                        case '\\': out += '\\'; break;
                        case '/': out += '/'; break;
                        default: out += c; break;
                    }
                    escaped = false;
                } else if (c == '\\') escaped = true;
                else if (c == '"') return out;
                else out += c;
            }
            return {};
        }
        return {};
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
    this->setTitle("Auto Deco - AI Chat");

    auto keyLabel = CCLabelBMFont::create("OpenAI API Key (saved on this device)", "goldFont.fnt");
    keyLabel->setScale(0.38f); keyLabel->setAnchorPoint({0.f, 0.5f});
    keyLabel->setPosition({22.f, 210.f}); m_mainLayer->addChild(keyLabel);

    m_keyInput = TextInput::create(310.f, "Paste API key here", "chatFont.fnt");
    m_keyInput->setPosition({180.f, 185.f});
    m_keyInput->setPasswordMode(true);
    m_keyInput->setCommonFilter(CommonFilter::Any);
    m_keyInput->setMaxCharCount(240);
    m_keyInput->setString(Mod::get()->getSavedValue<std::string>("openai_api_key", ""));
    m_mainLayer->addChild(m_keyInput);

    auto promptLabel = CCLabelBMFont::create("Describe what the AI should build / decorate", "goldFont.fnt");
    promptLabel->setScale(0.38f); promptLabel->setAnchorPoint({0.f, 0.5f});
    promptLabel->setPosition({22.f, 145.f}); m_mainLayer->addChild(promptLabel);

    m_promptInput = TextInput::create(310.f, "Example: buff gameplay, glow-style deco", "chatFont.fnt");
    m_promptInput->setPosition({180.f, 120.f});
    m_promptInput->setCommonFilter(CommonFilter::Any);
    m_promptInput->setMaxCharCount(500); m_mainLayer->addChild(m_promptInput);

    m_status = CCLabelBMFont::create("AI prepares a validated plan (up to 200 items).", "chatFont.fnt");
    m_status->setScale(0.42f); m_status->setPosition({180.f, 82.f});
    m_status->setWidth(320.f); m_mainLayer->addChild(m_status);

    auto menu = CCMenu::create(); menu->setPosition({0.f, 0.f});
    auto save = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("SAVE KEY", 86, 0, 0.58f, true, "goldFont.fnt", "GJ_button_01.png", 28.f),
        this, menu_selector(AIChatPopup::onSaveKey));
    save->setPosition({62.f, 34.f}); menu->addChild(save);
    auto send = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("ASK AI", 86, 0, 0.58f, true, "goldFont.fnt", "GJ_button_02.png", 28.f),
        this, menu_selector(AIChatPopup::onSend));
    send->setPosition({180.f, 34.f}); menu->addChild(send);
    auto apply = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("APPLY", 86, 0, 0.58f, true, "goldFont.fnt", "GJ_button_03.png", 28.f),
        this, menu_selector(AIChatPopup::onApply));
    apply->setPosition({298.f, 34.f}); menu->addChild(apply);
    m_mainLayer->addChild(menu);
    return true;
}

void AIChatPopup::onSaveKey(CCObject*) {
    std::string key = m_keyInput->getString().c_str();
    if (key.empty()) { m_status->setString("Enter your API key first."); return; }
    Mod::get()->setSavedValue<std::string>("openai_api_key", key);
    Mod::get()->saveData();
    m_status->setString("API key saved on this device.");
}

void AIChatPopup::onSend(CCObject*) {
    std::string key = m_keyInput->getString().c_str();
    std::string prompt = m_promptInput->getString().c_str();
    if (key.empty()) { m_status->setString("Enter your OpenAI API key first."); return; }
    if (prompt.empty()) { m_status->setString("Type a build/deco request first."); return; }

    Mod::get()->setSavedValue<std::string>("openai_api_key", key);
    Mod::get()->saveData();
    m_plan.clear();
    m_validatedPlan = auto_deco::Plan{};
    m_status->setString("AI is analyzing the level and planning...");

    matjson::Value body = matjson::Value::object();
    body["model"] = "gpt-5.5";
    body["store"] = false;
    body["max_output_tokens"] = 12000;
    matjson::Value textFormat = matjson::Value::object();
    textFormat["format"] = matjson::Value::object();
    textFormat["format"]["type"] = "json_object";
    body["text"] = textFormat;
    body["instructions"] =
        "You are an expert Geometry Dash Editor AI. Preserve the recognizable existing layout while adding coherent gameplay and decoration. "
        "Aim for Demon-style challenge only when the user's request calls for it; never claim verified Demon difficulty without playtesting. "
        "Use trigger plans only when properties are explicitly supported in the plan. Return JSON only: "
        "{\"message\":\"short answer in the user's language\",\"placements\":["
        "{\"kind\":\"object\",\"id\":1,\"x\":100,\"y\":100,\"scale\":1,\"rotation\":0},"
        "{\"kind\":\"trigger\",\"triggerType\":\"move\",\"id\":901,\"x\":100,\"y\":100,\"groupID\":1,\"targetGroupID\":2,\"moveX\":30,\"moveY\":0,\"duration\":1,\"easing\":0}, "
        "{\"kind\":\"trigger\",\"triggerType\":\"color\",\"id\":899,\"x\":100,\"y\":100,\"groupID\":1,\"colorID\":1,\"colorR\":80,\"colorG\":180,\"colorB\":255}]}. "
        "Currently implement only move (targetGroupID, moveX, moveY, duration, easing) and color (groupID, colorID 1..999, colorR, colorG, colorB each 0..255, duration). Use move object ID 901 and color object ID 899. Do not output alpha or toggle triggers. "
        "At most 200 items. All values must be finite. IDs 1..5000. Coordinates must be inside bounds. Trigger group IDs and target IDs 0..9999, "
        "duration 0..600, easing 0..99. Do not invent unsupported trigger properties or output markdown.";

    std::ostringstream snapshot;
    auto existing = m_editorLayer ? m_editorLayer->getAllObjects() : nullptr;
    int seen = 0;
    if (existing) {
        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(existing)) {
            if (!object || seen >= 250) continue;
            auto pos = object->getPosition();
            snapshot << "{id:" << object->m_objectID << ",x:" << pos.x << ",y:" << pos.y
                     << ",rotation:" << object->getRotation() << ",scale:" << object->getScale() << "};";
            ++seen;
        }
    }
    body["input"] = prompt + "\nExisting level snapshot (" + std::to_string(seen) +
        " of up to 250 objects; object ID and transform only): " + snapshot.str() +
        "\nEditor bounds: x=[" + std::to_string(m_minX) + "," + std::to_string(m_maxX) +
        "], y=[" + std::to_string(m_minY) + "," + std::to_string(m_maxY) +
        "]. Analyze the current layout; return gameplay and decoration additions together.";

    auto request = web::WebRequest()
        .header("Authorization", std::string("Bearer ") + key)
        .header("Content-Type", "application/json")
        .bodyJSON(body)
        .timeout(std::chrono::seconds(60));

    Ref<AIChatPopup> self(this);
    async::spawn(request.post("https://api.openai.com/v1/responses"), [self](web::WebResponse response) {
        if (!self) return;
        auto raw = response.string();
        if (!raw) { self->m_status->setString("Network error. Check internet connection."); return; }
        if (!response.ok()) {
            // Show the API's actual error message instead of only its HTTP status.
            // Keep the text bounded so a large server response cannot flood the popup.
            std::string errorBody = raw.unwrap();
            if (errorBody.size() > 280) errorBody.resize(280);
            std::string status = "OpenAI API error HTTP " + std::to_string(response.code()) + ": " + errorBody;
            self->m_status->setString(status.c_str());
            return;
        }

        auto answer = extractResponseText(raw.unwrap());
        auto_deco::Plan parsed;
        if (!auto_deco::parsePlanJSON(answer, parsed)) {
            self->m_status->setString("AI response failed strict JSON validation. Nothing was applied.");
            return;
        }

        for (auto& item : parsed.items) {
            if (item.x < self->m_minX || item.x > self->m_maxX ||
                item.y < self->m_minY || item.y > self->m_maxY) {
                self->m_status->setString("Plan contains coordinates outside the editor bounds. Nothing was applied.");
                return;
            }
        }

        self->m_validatedPlan = std::move(parsed);
        for (auto const& item : self->m_validatedPlan.items) {
            if (item.kind != auto_deco::PlannedObject::Kind::Object) continue;
            Placement p; p.id = item.id; p.x = item.x; p.y = item.y;
            p.scale = item.scale; p.rotation = item.rotation;
            self->m_plan.push_back(p);
        }

        int triggers = static_cast<int>(self->m_validatedPlan.items.size() - self->m_plan.size());
        self->m_status->setString(("Validated plan: " + std::to_string(self->m_plan.size()) +
            " regular objects, " + std::to_string(triggers) +
            " trigger specifications. Move and Color are ready to Apply.").c_str());
    });
}

void AIChatPopup::onApply(CCObject*) {
    if (!m_editorLayer) { m_status->setString("Editor is no longer available."); return; }
    if (m_validatedPlan.items.empty()) { m_status->setString("Ask AI first to generate a plan."); return; }

    for (auto const& item : m_validatedPlan.items) {
        if (item.kind == auto_deco::PlannedObject::Kind::Trigger &&
            item.triggerType != auto_deco::PlannedObject::TriggerType::Move &&
            item.triggerType != auto_deco::PlannedObject::TriggerType::Color) {
            m_status->setString("Plan includes an unsupported trigger. Nothing was applied.");
            return;
        }
    }

    int made = 0, triggersMade = 0;
    for (auto const& item : m_validatedPlan.items) {
        if (item.kind == auto_deco::PlannedObject::Kind::Object) {
            auto object = m_editorLayer->createObject(item.id, {item.x, item.y}, false);
            if (!object) continue;
            object->setScale(item.scale);
            object->setRotation(item.rotation);
            ++made;
            continue;
        }

        int triggerID = item.triggerType == auto_deco::PlannedObject::TriggerType::Move ? 901 : 899;
        auto base = m_editorLayer->createObject(triggerID, {item.x, item.y}, false);
        auto trigger = typeinfo_cast<EffectGameObject*>(base);
        if (!trigger) {
            if (base) base->removeFromParentAndCleanup(true);
            continue;
        }
        trigger->setTargetID(item.triggerType == auto_deco::PlannedObject::TriggerType::Move
            ? item.targetGroupID : item.groupID);
        trigger->m_duration = item.duration;
        if (item.triggerType == auto_deco::PlannedObject::TriggerType::Move) {
            trigger->m_moveOffset = cocos2d::CCPoint(item.moveX, item.moveY);
            trigger->m_easingType = static_cast<EasingType>(item.easing);
        } else {
            trigger->m_targetColor = item.colorID;
            trigger->m_triggerTargetColor = cocos2d::ccColor3B{
                static_cast<GLubyte>(item.colorR),
                static_cast<GLubyte>(item.colorG),
                static_cast<GLubyte>(item.colorB)
            };
        }
        ++triggersMade;
    }
    m_plan.clear();
    m_validatedPlan = auto_deco::Plan{};
    m_status->setString(("Applied " + std::to_string(made) + " objects and " + std::to_string(triggersMade) + " Move/Color triggers. Use Undo to revert.").c_str());
}
