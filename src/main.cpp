#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include "AIChatPopup.hpp"

using namespace geode::prelude;

namespace {
    struct Bounds {
        float minX = 0.f, maxX = 0.f, minY = 0.f, maxY = 0.f;
        bool valid = false;
    };

    Bounds getBounds(cocos2d::CCArray* objects) {
        Bounds b;
        if (!objects || objects->count() == 0) return b;
        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(objects)) {
            if (!object) continue;
            auto p = object->getPosition();
            if (!b.valid) {
                b.minX = b.maxX = p.x;
                b.minY = b.maxY = p.y;
                b.valid = true;
            } else {
                b.minX = std::min(b.minX, p.x);
                b.maxX = std::max(b.maxX, p.x);
                b.minY = std::min(b.minY, p.y);
                b.maxY = std::max(b.maxY, p.y);
            }
        }
        return b;
    }
}

class $modify(AutoDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto size = cocos2d::CCDirector::sharedDirector()->getWinSize();

        auto menu = cocos2d::CCMenu::create();
        if (!menu) return true;

        auto chatSprite = ButtonSprite::create("AI CHAT", 76, 0, 0.65f, true, "goldFont.fnt", "GJ_button_04.png", 28.f);
        auto chatButton = CCMenuItemSpriteExtra::create(chatSprite, this, menu_selector(AutoDecoEditorUI::onAIChat));
        if (chatButton) {
            chatButton->setPosition({size.width - 52.f, size.height - 110.f});
            menu->addChild(chatButton);
        }
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 1000);
        return true;
    }

    cocos2d::CCArray* getTargetObjects() {
        if (m_selectedObjects && m_selectedObjects->count() > 0) return m_selectedObjects;
        if (m_selectedObject) return cocos2d::CCArray::createWithObject(m_selectedObject);
        return m_editorLayer ? m_editorLayer->getAllObjects() : nullptr;
    }

    Bounds getTargetBounds() {
        auto bounds = getBounds(getTargetObjects());
        if (bounds.valid && bounds.maxX - bounds.minX < 240.f) {
            auto center = (bounds.minX + bounds.maxX) * 0.5f;
            bounds.minX = center - 180.f;
            bounds.maxX = center + 180.f;
        }
        return bounds;
    }

    void onAIChat(cocos2d::CCObject*) {
        auto bounds = getTargetBounds();
        if (!m_editorLayer || !bounds.valid) {
            FLAlertLayer::create("AI Chat", "Add a layout first so AI has an area to work in.", "OK")->show();
            return;
        }
        AIChatPopup::create(m_editorLayer, bounds.minX, bounds.maxX, bounds.minY, bounds.maxY)->show();
    }


};

$on_mod(Loaded) {
    log::info("Auto Deco AI loaded: chat-only editor assistant ready");
}
