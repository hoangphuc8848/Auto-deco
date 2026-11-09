#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace gdautobot {
 enum class Mode : uint8_t { Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing, Unknown };
 struct FailureSample {
  float playerX=0.f, playerY=0.f, speed=0.f;
  uint8_t action=0;
  uint32_t failures=0, successes=0;
 };

 // Stores situation statistics, never a replay of button timings.
 // Statistics survive game restarts through Geode's saved-value storage.
 class LearningMemory {
 public:
  LearningMemory() {
   auto mod = geode::Mod::get();
   auto keys = mod->getSavedValue<std::vector<std::string>>("memory.keys", {});
   for (auto const& key : keys) {
    FailureSample s;
    s.failures = mod->getSavedValue<uint32_t>("memory." + key + ".fail", 0);
    s.successes = mod->getSavedValue<uint32_t>("memory." + key + ".success", 0);
    m_samples.emplace(key, s);
   }
  }

  void recordFailure(std::string const& key, FailureSample sample) {
   auto& e=m_samples[key];
   e.playerX=sample.playerX; e.playerY=sample.playerY;
   e.speed=sample.speed; e.action=sample.action; ++e.failures;
   persist(key,e);
  }
  void recordSuccess(std::string const& key) {
   auto& e=m_samples[key]; ++e.successes; persist(key,e);
  }
  FailureSample const* find(std::string const& key) const {
   auto i=m_samples.find(key);
   return i==m_samples.end()?nullptr:&i->second;
  }
  std::unordered_map<std::string,FailureSample> const& samples() const { return m_samples; }

 private:
  void persist(std::string const& key, FailureSample const& e) {
   auto mod = geode::Mod::get();
   auto keys = mod->getSavedValue<std::vector<std::string>>("memory.keys", {});
   if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
    keys.push_back(key);
    mod->setSavedValue("memory.keys", keys);
   }
   mod->setSavedValue("memory." + key + ".fail", e.failures);
   mod->setSavedValue("memory." + key + ".success", e.successes);
  }
  std::unordered_map<std::string,FailureSample> m_samples;
 };
}
