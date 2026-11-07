#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
namespace gdautobot {
 enum class Mode : uint8_t { Cube, Ship, Ball, UFO, Wave, Robot, Spider, Unknown };
 struct FailureSample { float playerX=0.f, playerY=0.f, speed=0.f; uint8_t action=0; uint32_t failures=0, successes=0; };
 // Situation/outcome memory, not a recorded input timeline.
 class LearningMemory {
 public:
  void recordFailure(std::string const& key, FailureSample sample) {
   auto& e=m_samples[key]; e.playerX=sample.playerX; e.playerY=sample.playerY; e.speed=sample.speed; e.action=sample.action; ++e.failures;
  }
  void recordSuccess(std::string const& key) { ++m_samples[key].successes; }
  FailureSample const* find(std::string const& key) const { auto i=m_samples.find(key); return i==m_samples.end()?nullptr:&i->second; }
  std::unordered_map<std::string,FailureSample> const& samples() const { return m_samples; }
 private: std::unordered_map<std::string,FailureSample> m_samples;
};
}
