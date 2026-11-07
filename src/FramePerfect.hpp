#pragma once
#include <cstdint>
#include <algorithm>

namespace gdautobot {

// Frame-domain input planner. One game update is one simulation frame.
// The caller must supply measured observations and execute the returned edge.
enum class InputEdge : uint8_t { Hold, Press, Release };

struct FrameWindow {
    int earliest = 0;
    int latest = 0;
    int target = 0;
};

class FramePerfectPlanner {
public:
    // Select the center of a safe input window, clamped to the measured window.
    // A zero/invalid window is rejected rather than guessed.
    static bool chooseWindow(int earliest, int latest, FrameWindow& out) {
        if (earliest < 0 || latest < earliest) return false;
        out.earliest = earliest;
        out.latest = latest;
        out.target = earliest + (latest - earliest) / 2;
        return true;
    }

    // Produces an input edge exactly on the requested simulation frame.
    // It never repeats a press while already held.
    InputEdge update(int currentFrame, int targetFrame, bool shouldHold) {
        if (currentFrame < targetFrame) return m_held ? InputEdge::Hold : InputEdge::Release;
        if (currentFrame > targetFrame) return m_held ? InputEdge::Hold : InputEdge::Release;

        if (shouldHold && !m_held) {
            m_held = true;
            return InputEdge::Press;
        }
        if (!shouldHold && m_held) {
            m_held = false;
            return InputEdge::Release;
        }
        return m_held ? InputEdge::Hold : InputEdge::Release;
    }

    InputEdge releaseNow() {
        if (!m_held) return InputEdge::Release;
        m_held = false;
        return InputEdge::Release;
    }

    bool isHeld() const { return m_held; }

private:
    bool m_held = false;
};

} // namespace gdautobot
