#pragma once
#include "LearningMemory.hpp"
namespace gdautobot {
 struct Observation {
  Mode mode=Mode::Unknown;
  float playerX=0.f, playerY=0.f, horizontalSpeed=0.f;
  float nearestHazardX=0.f, nearestHazardY=0.f, hazardWidth=0.f;
  bool grounded=false, hazardVisible=false;
 };
 enum class Action : uint8_t { None, Press, Release };
 // Conservative baseline: never inject input until perception and prediction are validated.
 class BotController {
 public:
  Action decide(Observation const&, LearningMemory const&) const { return Action::None; }
 };
}
