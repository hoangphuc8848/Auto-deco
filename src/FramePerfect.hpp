#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace gdautobot {

// Timing calibration primitive. This improves repeatability only after a caller
// supplies measured safe windows and reports real in-game outcomes.
enum class InputEdge : uint8_t { Hold, Press, Release };

struct FrameWindow {
    int earliest = 0;
    int latest = -1;
    int target = 0;
};

class FramePerfectPlanner {
public:
    static bool chooseWindow(int earliest, int latest, FrameWindow& out) {
        if (earliest < 0 || latest < earliest) return false;
        out.earliest = earliest;
        out.latest = latest;
        out.target = earliest + (latest - earliest) / 2;
        return true;
    }

    // Apply a learned timing correction, but never leave the measured safe window.
    int calibratedTarget(FrameWindow const& window) const {
        if (window.latest < window.earliest) return -1;
        return std::clamp(window.target + m_offset, window.earliest, window.latest);
    }

    // Call once per simulation update. A missed target is discarded, not fired late.
    InputEdge update(int currentFrame, int targetFrame, bool shouldHold) {
        if (targetFrame < 0 || currentFrame < targetFrame)
            return m_held ? InputEdge::Hold : InputEdge::Release;

        if (currentFrame > targetFrame) {
            m_missedTarget = true;
            return m_held ? InputEdge::Hold : InputEdge::Release;
        }

        if (shouldHold && !m_held) {
            m_held = true;
            m_missedTarget = false;
            return InputEdge::Press;
        }
        if (!shouldHold && m_held) {
            m_held = false;
            m_missedTarget = false;
            return InputEdge::Release;
        }
        return m_held ? InputEdge::Hold : InputEdge::Release;
    }

    // Feed measured outcomes from actual gameplay. A success reinforces the
    // current offset; a failure nudges one frame in the supplied correction direction.
    void reportOutcome(bool success, int correctionDirection = 0) {
        if (success) {
            m_successes++;
            m_confidence = std::min(1.f, m_confidence + 0.025f);
            return;
        }
        m_failures++;
        m_confidence = std::max(0.f, m_confidence - 0.05f);
        if (correctionDirection != 0)
            m_offset = std::clamp(m_offset + (correctionDirection > 0 ? 1 : -1), -3, 3);
    }

    void reset() {
        m_held = false;
        m_missedTarget = false;
        m_offset = 0;
        m_successes = 0;
        m_failures = 0;
        m_confidence = 0.5f;
    }

    InputEdge releaseNow() {
        if (!m_held) return InputEdge::Release;
        m_held = false;
        return InputEdge::Release;
    }

    bool isHeld() const { return m_held; }
    bool missedTarget() const { return m_missedTarget; }
    int timingOffset() const { return m_offset; }
    float confidence() const { return m_confidence; }
    uint32_t successes() const { return m_successes; }
    uint32_t failures() const { return m_failures; }

private:
    bool m_held = false;
    bool m_missedTarget = false;
    int m_offset = 0;
    uint32_t m_successes = 0;
    uint32_t m_failures = 0;
    float m_confidence = 0.5f;
};

} // namespace gdautobot
