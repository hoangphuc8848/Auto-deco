#pragma once
#include "LearningMemory.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace gdautobot {
 struct Observation {
  Mode mode=Mode::Unknown;
  float playerX=0.f, playerY=0.f, horizontalSpeed=0.f;
  float nearestHazardX=0.f, nearestHazardY=0.f, hazardWidth=0.f;
  float safeMinY=0.f, safeMaxY=0.f;
  bool grounded=false, hazardVisible=false, boundsKnown=false;
 };
 enum class Action : uint8_t { None, Press, Release };

 // Deterministic reactive controller. It requires live observations from the game;
 // it deliberately does not guess when obstacle/bounds data are unavailable.
 class BotController {
 public:
  Action decide(Observation const& o, LearningMemory const& memory) {
   if (o.mode == Mode::Unknown || !std::isfinite(o.playerY)) return Action::None;

   if (o.mode == Mode::Wave) {
    if (!o.boundsKnown || !std::isfinite(o.safeMinY) ||
        !std::isfinite(o.safeMaxY) || o.safeMaxY <= o.safeMinY)
     return Action::None;

    float target = (o.safeMinY + o.safeMaxY) * 0.5f;
    if (o.hazardVisible && o.nearestHazardX >= o.playerX) {
     // Bias away from the observed hazard while keeping a safety margin.
     float direction = o.nearestHazardY >= o.playerY ? -1.f : 1.f;
     target += direction * (o.safeMaxY - o.safeMinY) * 0.22f;
    }

    // Shift away from regions where this situation repeatedly caused failure.
    auto key = situationKey(o);
    if (auto sample = memory.find(key); sample && sample->failures > sample->successes)
     target += (sample->playerY >= o.playerY ? -1.f : 1.f) *
               (o.safeMaxY - o.safeMinY) * 0.10f;

    target = std::clamp(target, o.safeMinY, o.safeMaxY);
    // Wave rises while held and falls while released.
    m_lastAction = (target > o.playerY) ? Action::Press : Action::Release;
    return m_lastAction;
   }

   if (o.mode == Mode::Cube || o.mode == Mode::Robot) {
    if (!o.hazardVisible) return Action::Release;
    float distance = o.nearestHazardX - o.playerX;
    if (distance < 0.f) return Action::Release;
    float reaction = std::max(12.f, o.horizontalSpeed * 5.f);
    return distance <= reaction ? Action::Press : Action::Release;
   }
   return Action::None;
  }

  static std::string situationKey(Observation const& o) {
   auto bin=[](float v,float step) { return static_cast<int>(std::floor(v/step)); };
   return std::to_string(static_cast<unsigned>(o.mode)) + ":" +
          std::to_string(bin(o.playerY, 20.f)) + ":" +
          std::to_string(bin(o.horizontalSpeed, 2.f));
  }
 private:
  Action m_lastAction=Action::None;
 };
}
