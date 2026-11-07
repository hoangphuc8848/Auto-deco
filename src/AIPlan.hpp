#pragma once

#include <Geode/Geode.hpp>
#include <cmath>
#include <string>
#include <vector>

namespace auto_deco {

struct PlannedObject {
    enum class Kind { Object, Trigger };
    enum class TriggerType { Alpha, Move, Toggle, Color };
    Kind kind = Kind::Object;
    TriggerType triggerType = TriggerType::Alpha;
    int id = 1;
    float x = 0.f, y = 0.f, scale = 1.f, rotation = 0.f;
    int groupID = 0, targetGroupID = 0, colorID = 1;
    bool activateGroup = false;
    float duration = 0.f, moveX = 0.f, moveY = 0.f, opacity = 1.f;
    int easing = 0;
    matjson::Value properties = matjson::Value::object();
};

struct Plan {
    std::string message;
    std::vector<PlannedObject> items;
};

inline bool readFiniteNumber(matjson::Value const& value, char const* key, float& out) {
    if (!value.isObject() || !value.contains(key) || !value[key].isNumber()) return false;
    auto result = value[key].asDouble();
    if (!result) return false;
    double number = result.unwrap();
    if (!std::isfinite(number) || number < -1000000.0 || number > 1000000.0) return false;
    out = static_cast<float>(number);
    return true;
}

inline bool readBoundedInt(matjson::Value const& value, char const* key, int min, int max, int& out) {
    float number = 0.f;
    if (!readFiniteNumber(value, key, number)) return false;
    if (std::floor(number) != number || number < min || number > max) return false;
    out = static_cast<int>(number);
    return true;
}

inline bool parsePlanJSON(std::string const& text, Plan& out) {
    auto parsed = matjson::parse(text);
    if (!parsed || !parsed.unwrap().isObject()) return false;

    auto root = parsed.unwrap();
    if (!root.contains("placements") || !root["placements"].isArray()) return false;

    Plan candidate;
    if (root.contains("message") && root["message"].isString()) {
        auto message = root["message"].asString();
        if (!message) return false;
        candidate.message = message.unwrap();
    }

    auto const& items = root["placements"];
    if (items.size() == 0 || items.size() > 200) return false;

    for (auto const& item : items) {
        if (!item.isObject()) return false;
        PlannedObject p;

        if (item.contains("kind")) {
            if (!item["kind"].isString()) return false;
            auto kindResult = item["kind"].asString();
            if (!kindResult) return false;
            auto kind = kindResult.unwrap();
            if (kind == "trigger") p.kind = PlannedObject::Kind::Trigger;
            else if (kind != "object") return false;
        }

        if (!readBoundedInt(item, "id", 1, 5000, p.id) ||
            !readFiniteNumber(item, "x", p.x) ||
            !readFiniteNumber(item, "y", p.y)) return false;

        if (item.contains("scale") && !readFiniteNumber(item, "scale", p.scale)) return false;
        if (item.contains("rotation") && !readFiniteNumber(item, "rotation", p.rotation)) return false;
        if (p.scale < 0.25f || p.scale > 2.5f || p.rotation < -360.f || p.rotation > 360.f) return false;

        if (p.kind == PlannedObject::Kind::Trigger) {
            if (!readBoundedInt(item, "groupID", 0, 9999, p.groupID) ||
                !readBoundedInt(item, "targetGroupID", 0, 9999, p.targetGroupID)) return false;
            if (item.contains("duration") && !readFiniteNumber(item, "duration", p.duration)) return false;
            if (item.contains("easing") && !readBoundedInt(item, "easing", 0, 99, p.easing)) return false;
            if (item.contains("properties")) {
                if (!item["properties"].isObject()) return false;
                p.properties = item["properties"];
            }
            if (p.duration < 0.f || p.duration > 600.f) return false;
        }

        candidate.items.push_back(std::move(p));
    }

    out = std::move(candidate);
    return true;
}

} // namespace auto_deco
