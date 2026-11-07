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
        auto key = body.find("\"text\":");
        if (key == std::string::npos) return {};
        auto start = body.find('"', key + 7);
        if (start == std::string::npos) return {};
        ++start;
        std::string out;
        bool escaped = false;
        for (size_t i = start; i < body.size(); ++i) {
            char c = body[i];
            if (escaped) {
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += c; break;
                }
                escaped = false;
            } else if (c == '\\') escaped = true;
            else if (c == '"') break;
            else out += c;
        }
        return out;
    }

    bool readNumber(std::string const& object, char const* key, float& value) {
        std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
        std::smatch match;
        if (!std::regex_search(object, match, pattern)) return false;
        try { value = std::stof(match[1].str()); return std::isfinite(value); }
        catch (...) { return false; }
    }

    bool parsePlan(std::string const& text, std::vector<AIChatPopup::Placement>& out) {
        // Find the placements array, then scan JSON structurally. The former
        // regex stopped at the first ']' and failed on nested values/strings.
        auto key = text.find("\\\"placements\\\"");
        if (key == std::string::npos) return false;
        auto arrayStart = text.find('[', key);
        if (arrayStart == std::string::npos) return false;

        bool inString = false;
        bool escaped = false;
        int objectDepth = 0;
        size_t objectStart = std::string::npos;
        for (size_t i = arrayStart + 1; i < text.size(); ++i) {
            char c = text[i];
            if (inString) {
                if (escaped) escaped = false;
                else if (c == '\\\\') escaped = true;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') { inString = true; continue; }
            if (c == '{') {
                if (objectDepth == 0) objectStart = i;
                ++objectDepth;
            } else if (c == '}') {
                if (objectDepth <= 0) return false;
                --objectDepth;
                if (objectDepth == 0 && objectStart != std::string::npos) {
                    auto item = text.substr(objectStart, i - objectStart + 1);
                    float id = 0, x = 0, y = 0, scale = 1, rotation = 0;
                    if (readNumber(item, "id", id) &&
                        readNumber(item, "x", x) &&
                        readNumber(item, "y", y)) {
                        readNumber(item, "scale", scale);
                        readNumber(item, "rotation", rotation);
                        AIChatPopup::Placement p;
                        p.id = static_cast<int>(id);
                        p.x = x;
                        p.y = y;
                        p.scale = std::clamp(scale, 0.25f, 2.5f);
                        p.rotation = std::clamp(rotation, -360.f, 360.f);
                        if (p.id >= 1 && p.id <= 5000 &&
                            std::isfinite(p.x) && std::isfinite(p.y)) {
                            out.push_back(p);
                            if (out.size() >= 200) break;
                        }
                    }
                    objectStart = std::string::npos;
                }
            } else if (c == ']' && objectDepth == 0) {
                break;
            }
        }
        return !out.empty();
    }
}

AIChatPopup* AIChatPopup::create(LevelEditorLayer* editor, float minX, float maxX, float minY, float maxY) {
    auto ret = new AIChatPopup();
    ret->m_editorLayer = editor;
    ret->m_minX = minX; ret->m_maxX = maxX;
    ret->m_minY = minY; ret->m_maxY = maxY;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
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
    m_keyInput->setCommonFilter(CommonFilter::Any); // Allow API key characters such as '-', '_', and '.'
    m_keyInput->setMaxCharCount(240);
    m_keyInput->setString(Mod::get()->getSavedValue<std::string>("openai_api_key", ""));
    m_mainLayer->addChild(m_keyInput);

    auto promptLabel = CCLabelBMFont::create("Describe what the AI should build / decorate", "goldFont.fnt");
    promptLabel->setScale(0.38f); promptLabel->setAnchorPoint({0.f, 0.5f});
    promptLabel->setPosition({22.f, 145.f}); m_mainLayer->addChild(promptLabel);

    m_promptInput = TextInput::create(310.f, "Example: glow-style decoration, dark blue", "chatFont.fnt");
    m_promptInput->setPosition({180.f, 120.f});
    m_promptInput->setCommonFilter(CommonFilter::Any);
    m_promptInput->setMaxCharCount(500); m_mainLayer->addChild(m_promptInput);

    m_status = CCLabelBMFont::create("Enter a request. AI will prepare up to 100 objects.", "chatFont.fnt");
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
    m_status->setString("AI is planning objects...");

    matjson::Value body = matjson::Value::object();
    body["model"] = "gpt-5.5";
    body["store"] = false;
    body["instructions"] =
        "You are an expert Geometry Dash Demon level editor assistant. Automatically inspect the supplied existing-level snapshot; "
        "do not pretend to see pixels or objects that are not in the snapshot. Infer the current layout from object IDs and coordinates. "
        "Upgrade the EXISTING level rather than replacing it: preserve its recognizable structure, fill gameplay gaps, improve jumps, "
        "hazard spacing and rhythm, and add coherent theme-matched decoration. Aim for a challenging Demon-style layout, but prioritize "
        "playability and readable hitboxes over unfair or impossible gameplay. Add BOTH gameplay objects and decoration in the same plan. "
        "Avoid duplicate spam, random clutter, and placing decoration over gameplay hazards. Reply ONLY with one JSON object: "
        "{\\\"message\\\":\\\"short explanation in the user's language\\\",\\\"placements\\\":[{\\\"id\\\":1,\\\"x\\\":100,\\\"y\\\":100,\\\"scale\\\":1,\\\"rotation\\\":0}]}. "
        "Use at most 200 placements. Use valid Geometry Dash editor object IDs from 1 to 5000, choosing known standard gameplay/deco objects; "
        "never invent IDs. Keep all coordinates inside the supplied bounds. No markdown, scripts, or commands.";
    std::ostringstream snapshot;
    auto existing = m_editorLayer ? m_editorLayer->getAllObjects() : nullptr;
    int seen = 0;
    if (existing) {
        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(existing)) {
            if (!object || seen >= 250) continue;
            auto pos = object->getPosition();
            snapshot << "{id:" << object->m_objectID
                     << ",x:" << pos.x << ",y:" << pos.y
                     << ",rotation:" << object->getRotation()
                     << ",scale:" << object->getScale() << "};";
            ++seen;
        }
    }
    body["input"] = prompt + "\nExisting level snapshot (" + std::to_string(seen) +
        " of up to 250 objects; each entry is object ID and transform): " + snapshot.str() +
        "\nEditor bounds: x=[" + std::to_string(m_minX) + "," +
        std::to_string(m_maxX) + "], y=[" + std::to_string(m_minY) + "," + std::to_string(m_maxY) +
        "]. Analyze the snapshot, then return additions that buff gameplay and decoration together.";

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
            self->m_status->setString(("OpenAI API error: HTTP " + std::to_string(response.code())).c_str());
            return;
        }
        auto answer = extractResponseText(raw.unwrap());
        std::vector<Placement> plan;
        if (!parsePlan(answer, plan)) {
            self->m_status->setString("AI replied, but no valid object plan was found. Try a clearer request.");
            return;
        }
        for (auto& p : plan) {
            p.x = std::clamp(p.x, self->m_minX, self->m_maxX);
            p.y = std::clamp(p.y, self->m_minY, self->m_maxY);
        }
        self->m_plan = std::move(plan);
        self->m_status->setString(("Plan ready: " + std::to_string(self->m_plan.size()) +
            " objects. Press APPLY to create them.").c_str());
    });
}

void AIChatPopup::onApply(CCObject*) {
    if (!m_editorLayer) { m_status->setString("Editor is no longer available."); return; }
    if (m_plan.empty()) { m_status->setString("Ask AI first to generate a plan."); return; }

    int made = 0;
    for (auto const& p : m_plan) {
        auto object = m_editorLayer->createObject(p.id, {p.x, p.y}, false);
        if (!object) continue;
        object->setScale(p.scale);
        object->setRotation(p.rotation);
        ++made;
    }
    m_plan.clear();
    m_status->setString(("Applied " + std::to_string(made) + " objects. Use Undo to revert.").c_str());
}
