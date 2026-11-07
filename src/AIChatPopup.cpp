#include "AIChatPopup.hpp"
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

namespace {
    std::string extractText(std::string const& body) {
        // Responses API returns output[].content[].text. Extract the first text field
        // without trusting or executing any model-generated content.
        auto key = body.find("\"text\":");
        if (key == std::string::npos) return body.substr(0, 1200);
        auto start = body.find('"', key + 7);
        if (start == std::string::npos) return body.substr(0, 1200);
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
            if (out.size() >= 1200) break;
        }
        return out.empty() ? body.substr(0, 1200) : out;
    }
}

AIChatPopup* AIChatPopup::create() {
    auto ret = new AIChatPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AIChatPopup::init() {
    if (!Popup::init(360.f, 245.f, "GJ_square01.png")) return false;
    this->setTitle("Auto Deco - AI Chat");

    auto keyLabel = CCLabelBMFont::create("OpenAI API Key (stored on this device)", "goldFont.fnt");
    keyLabel->setScale(0.38f);
    keyLabel->setAnchorPoint({0.f, 0.5f});
    keyLabel->setPosition({22.f, 190.f});
    m_mainLayer->addChild(keyLabel);

    m_keyInput = TextInput::create(310.f, "Paste API key here", "chatFont.fnt");
    m_keyInput->setPosition({180.f, 165.f});
    m_keyInput->setPasswordMode(true);
    m_keyInput->setMaxCharCount(240);
    m_keyInput->setString(Mod::get()->getSavedValue<std::string>("openai_api_key", ""));
    m_mainLayer->addChild(m_keyInput);

    auto promptLabel = CCLabelBMFont::create("Tell the AI what to build or decorate", "goldFont.fnt");
    promptLabel->setScale(0.38f);
    promptLabel->setAnchorPoint({0.f, 0.5f});
    promptLabel->setPosition({22.f, 125.f});
    m_mainLayer->addChild(promptLabel);

    m_promptInput = TextInput::create(310.f, "Example: decorate this level in glow style", "chatFont.fnt");
    m_promptInput->setPosition({180.f, 100.f});
    m_promptInput->setCommonFilter(CommonFilter::Any);
    m_promptInput->setMaxCharCount(500);
    m_mainLayer->addChild(m_promptInput);

    m_status = CCLabelBMFont::create("Ready. API usage is billed separately by OpenAI.", "chatFont.fnt");
    m_status->setScale(0.42f);
    m_status->setPosition({180.f, 66.f});
    m_mainLayer->addChild(m_status);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    auto save = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("SAVE KEY", 90, 0, 0.6f, true, "goldFont.fnt", "GJ_button_01.png", 30.f),
        this, menu_selector(AIChatPopup::onSaveKey));
    save->setPosition({112.f, 27.f});
    menu->addChild(save);
    auto send = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("SEND", 90, 0, 0.6f, true, "goldFont.fnt", "GJ_button_02.png", 30.f),
        this, menu_selector(AIChatPopup::onSend));
    send->setPosition({248.f, 27.f});
    menu->addChild(send);
    m_mainLayer->addChild(menu);
    return true;
}

void AIChatPopup::onSaveKey(CCObject*) {
    auto key = m_keyInput->getString();
    if (key.empty()) {
        m_status->setString("Enter your API key first.");
        return;
    }
    Mod::get()->setSavedValue<std::string>("openai_api_key", key);
    Mod::get()->saveData();
    m_status->setString("API key saved locally on this device.");
}

void AIChatPopup::onSend(CCObject*) {
    auto key = m_keyInput->getString();
    auto prompt = m_promptInput->getString();
    if (key.empty()) {
        m_status->setString("Enter your OpenAI API key first.");
        return;
    }
    if (prompt.empty()) {
        m_status->setString("Type a request first.");
        return;
    }

    Mod::get()->setSavedValue<std::string>("openai_api_key", key);
    Mod::get()->saveData();
    m_status->setString("Thinking...");

    matjson::Value body = matjson::Value::object();
    body["model"] = "gpt-5.5";
    body["store"] = false;
    body["instructions"] = "You are an assistant inside the Geometry Dash level editor. Reply in the user's language. Explain how to build or decorate a level. Do not claim you changed the level; this chat currently only returns advice.";
    body["input"] = prompt;

    auto request = web::WebRequest()
        .header("Authorization", "Bearer " + key)
        .header("Content-Type", "application/json")
        .bodyJSON(body)
        .timeout(std::chrono::seconds(60));

    Ref<AIChatPopup> self(this);
    async::spawn(request.post("https://api.openai.com/v1/responses"), [self](web::WebResponse response) {
        if (!self) return;
        auto raw = response.string();
        if (!raw) {
            self->m_status->setString("Network error. Check your connection and API key.");
            return;
        }
        if (!response.ok()) {
            self->m_status->setString(("OpenAI API error: " + std::to_string(response.code())).c_str());
            return;
        }
        auto answer = extractText(raw.unwrap());
        self->m_status->setString(answer.c_str());
    });
}
