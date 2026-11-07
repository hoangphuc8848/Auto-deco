#pragma once
#include <Geode/Geode.hpp>

class AIChatPopup : public geode::Popup {
protected:
    geode::TextInput* m_keyInput = nullptr;
    geode::TextInput* m_promptInput = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;

    bool init() override;
    void onSaveKey(cocos2d::CCObject*);
    void onSend(cocos2d::CCObject*);

public:
    static AIChatPopup* create();
};
