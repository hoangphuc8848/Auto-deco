#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include "AIPlan.hpp"

class AIChatPopup : public geode::Popup {
public:
    struct Placement { int id = 1; float x = 0.f; float y = 0.f; float scale = 1.f; float rotation = 0.f; };
protected:
    LevelEditorLayer* m_editorLayer = nullptr;
    float m_minX = 0.f, m_maxX = 0.f, m_minY = 0.f, m_maxY = 0.f;
    geode::TextInput* m_keyInput = nullptr;
    geode::TextInput* m_promptInput = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    std::vector<Placement> m_plan;
    auto_deco::Plan m_validatedPlan;
    bool init() override;
    void onSaveKey(cocos2d::CCObject*);
    void onSend(cocos2d::CCObject*);
    void onApply(cocos2d::CCObject*);
public:
    static AIChatPopup* create(LevelEditorLayer* editor, float minX, float maxX, float minY, float maxY);
};
