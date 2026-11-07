#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <array>
#include <random>

using namespace geode::prelude;

namespace {
    constexpr std::array<int, 8> DECOR_OBJECTS = {
        85, 86, 87, 88, 113, 114, 115, 116
    };

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

        auto decoSprite = ButtonSprite::create("AI DECO", 76, 0, 0.65f, true, "goldFont.fnt", "GJ_button_01.png", 28.f);
        auto decoButton = CCMenuItemSpriteExtra::create(decoSprite, this, menu_selector(AutoDecoEditorUI::onAutoDeco));
        if (decoButton) {
            decoButton->setPosition({size.width - 52.f, size.height - 34.f});
            menu->addChild(decoButton);
        }

        auto buildSprite = ButtonSprite::create("AI BUILD", 76, 0, 0.65f, true, "goldFont.fnt", "GJ_button_02.png", 28.f);
        auto buildButton = CCMenuItemSpriteExtra::create(buildSprite, this, menu_selector(AutoDecoEditorUI::onAutoBuild));
        if (buildButton) {
            buildButton->setPosition({size.width - 52.f, size.height - 72.f});
            menu->addChild(buildButton);
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

    void onAutoDeco(cocos2d::CCObject*) {
        if (!m_editorLayer) {
            FLAlertLayer::create("Auto Deco AI", "Open a level in the editor first.", "OK")->show();
            return;
        }
        auto bounds = getTargetBounds();
        if (!bounds.valid) {
            FLAlertLayer::create("Auto Deco AI", "No objects found. Add a layout first, or select a region.", "OK")->show();
            return;
        }

        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> jitter(0.f, 1.f);
        std::uniform_real_distribution<float> height(24.f, 76.f);
        std::uniform_int_distribution<size_t> choice(0, DECOR_OBJECTS.size() - 1);
        std::uniform_real_distribution<float> scale(0.55f, 0.9f);
        std::uniform_real_distribution<float> rotation(0.f, 360.f);

        auto width = bounds.maxX - bounds.minX;
        auto count = std::clamp(static_cast<int>(width / 115.f), 5, 60);
        int made = 0;
        for (int i = 0; i < count; ++i) {
            float x = bounds.minX + width * ((i + jitter(rng)) / static_cast<float>(count));
            float y = bounds.maxY + height(rng);
            auto object = m_editorLayer->createObject(DECOR_OBJECTS[choice(rng)], {x, y}, false);
            if (!object) continue;
            object->setScale(scale(rng));
            object->setRotation(rotation(rng));
            ++made;
        }
        FLAlertLayer::create("Auto Deco AI",
            CCString::createWithFormat("Created %d random decorations. Use Undo to revert.", made)->getCString(),
            "OK")->show();
    }

    void onAutoBuild(cocos2d::CCObject*) {
        if (!m_editorLayer) {
            FLAlertLayer::create("Auto Build AI", "Open a level in the editor first.", "OK")->show();
            return;
        }
        auto bounds = getTargetBounds();
        if (!bounds.valid) {
            FLAlertLayer::create("Auto Build AI", "Select a region or add a layout object first.", "OK")->show();
            return;
        }

        // Procedural starter platform: vanilla solid block objects, spaced on the editor grid.
        // Selection bounds are used when a region is selected; otherwise the level bounds are used.
        const float grid = 30.f;
        float startX = std::floor(bounds.minX / grid) * grid;
        float endX = std::ceil(bounds.maxX / grid) * grid;
        float baseY = std::floor((bounds.minY - 60.f) / grid) * grid;
        int made = 0;
        int columns = std::clamp(static_cast<int>((endX - startX) / grid), 4, 80);

        for (int i = 0; i < columns; ++i) {
            float x = startX + i * grid;
            auto block = m_editorLayer->createObject(1, {x, baseY}, false);
            if (block) ++made;
        }

        FLAlertLayer::create("Auto Build AI",
            CCString::createWithFormat("Built a %d-block starter platform. Use Undo to revert.", made)->getCString(),
            "OK")->show();
    }
};

$on_mod(Loaded) {
    log::info("Auto Deco AI loaded: editor Deco and Build tools ready");
}
