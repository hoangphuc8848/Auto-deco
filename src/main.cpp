#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <array>
#include <random>

using namespace geode::prelude;

namespace {
    // Decorative-only object IDs from the vanilla Geometry Dash object catalogue.
    constexpr std::array<int, 8> DECOR_OBJECTS = {
        85, 86, 87, 88, // decorative gears
        113, 114, 115, 116 // decorative bricks / brick pieces
    };

    struct Bounds {
        float minX = 0.f;
        float maxX = 0.f;
        float minY = 0.f;
        float maxY = 0.f;
        bool valid = false;
    };

    Bounds getBounds(cocos2d::CCArray* objects) {
        Bounds bounds;
        if (!objects || objects->count() == 0) return bounds;

        for (auto object : geode::cocos::CCArrayExt<GameObject, false>(objects)) {
            if (!object) continue;
            auto pos = object->getPosition();
            if (!bounds.valid) {
                bounds.minX = bounds.maxX = pos.x;
                bounds.minY = bounds.maxY = pos.y;
                bounds.valid = true;
            } else {
                bounds.minX = std::min(bounds.minX, pos.x);
                bounds.maxX = std::max(bounds.maxX, pos.x);
                bounds.minY = std::min(bounds.minY, pos.y);
                bounds.maxY = std::max(bounds.maxY, pos.y);
            }
        }
        return bounds;
    }
}

class $modify(AutoDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        if (!menu) return true;

        auto sprite = ButtonSprite::create(
            "AUTO DECO", 82, 0, 0.7f, true,
            "goldFont.fnt", "GJ_button_01.png", 30.f
        );
        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(AutoDecoEditorUI::onAutoDeco)
        );
        if (!button) return true;

        button->setPosition({winSize.width - 55.f, winSize.height - 38.f});
        menu->addChild(button);
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 1000);
        return true;
    }

    void onAutoDeco(cocos2d::CCObject*) {
        if (!m_editorLayer) {
            FLAlertLayer::create("Auto Deco", "Open a level in the editor first.", "OK")->show();
            return;
        }

        // "Both" mode: a selection defines the target region; with no selection,
        // the generator uses the complete level bounds.
        cocos2d::CCArray* targetObjects = nullptr;
        if (m_selectedObjects && m_selectedObjects->count() > 0) {
            targetObjects = m_selectedObjects;
        } else if (m_selectedObject) {
            targetObjects = cocos2d::CCArray::createWithObject(m_selectedObject);
        } else {
            targetObjects = m_editorLayer->getAllObjects();
        }

        auto bounds = getBounds(targetObjects);
        if (!bounds.valid) {
            FLAlertLayer::create("Auto Deco", "This level has no objects to use as a decoration area.", "OK")->show();
            return;
        }

        // Give a single selected object a useful working width.
        if (bounds.maxX - bounds.minX < 240.f) {
            auto center = (bounds.minX + bounds.maxX) * 0.5f;
            bounds.minX = center - 180.f;
            bounds.maxX = center + 180.f;
        }

        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> xOffset(0.f, 1.f);
        std::uniform_real_distribution<float> yOffset(22.f, 82.f);
        std::uniform_int_distribution<size_t> objectChoice(0, DECOR_OBJECTS.size() - 1);
        std::uniform_real_distribution<float> scaleChoice(0.55f, 0.95f);
        std::uniform_real_distribution<float> rotationChoice(0.f, 360.f);

        auto width = bounds.maxX - bounds.minX;
        auto count = std::clamp(static_cast<int>(width / 105.f), 6, 80);
        int created = 0;

        for (int i = 0; i < count; ++i) {
            auto x = bounds.minX + width * ((static_cast<float>(i) + xOffset(rng)) / static_cast<float>(count));
            auto y = bounds.maxY + yOffset(rng);
            auto objectID = DECOR_OBJECTS[objectChoice(rng)];

            // LevelEditorLayer::createObject creates a vanilla editor object at
            // the requested level coordinate and registers it with the editor.
            auto object = m_editorLayer->createObject(objectID, {x, y});
            if (!object) continue;
            object->setScale(scaleChoice(rng));
            object->setRotation(rotationChoice(rng));
            ++created;
        }

        if (created > 0) {
            FLAlertLayer::create(
                "Auto Deco",
                CCString::createWithFormat(
                    "Generated %d random decorations. Use Undo if you want to revert them.",
                    created
                )->getCString(),
                "OK"
            )->show();
        } else {
            FLAlertLayer::create("Auto Deco", "No decorations were created. Try selecting a different region.", "OK")->show();
        }
    }
};

$on_mod(Loaded) {
    log::info("Auto Deco loaded - Random Decoration generator ready");
}
