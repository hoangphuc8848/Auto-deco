# GD AutoBot / Offline Level Assistant

Android64 Geode mod prototype for Geometry Dash.

## Implemented
- Local Vietnamese/English keyword parser. No AI API, network request, API key, or model download.
- Offline theme/palette planning for Tech, Hell, Space, Forest, and Nine Circles.
- Editor panel with Generate, APPLY DECO, BUILD, and TRIGGERS actions.
- APPLY DECO places a small sample frame using standard block objects.
- BUILD places a basic sample floor and spikes.
- TRIGGERS attempts to place Color, Move, Pulse, Alpha, Toggle, and Spawn trigger objects.
- Generated sample blocks are assigned Group ID 9000. The supported effect trigger objects are assigned target ID 9000 where the Geode object type supports it.
- Experimental Cube-only play controller. Other modes are not automated.

## Important limitations
- This is a prototype, not a GPT-level AI and not a complete automatic level decorator.
- Trigger placement does not mean every Special field is configured. Color channel, Move offsets/duration/easing, Pulse values, Alpha opacity/duration, Toggle/Spawn group behavior, and trigger timing may still need manual setup in Edit Object.
- Trigger IDs and object creation must be verified in the target Geometry Dash/Geode version. The mod has not yet been confirmed in-game on Android.
- The current deco/gameplay placement is a small sample, not a complete polished level generator. Use an empty test area and save a copy before testing.
- Compatibility depends on the Geode SDK version declared in mod.json and the installed Geode version.

## Offline/privacy
All assistant planning logic runs locally in C++. No AI API, API key, telemetry, network calls, or model downloads are used by the assistant.

## Build
GitHub Actions builds Android64 and uploads the .geode artifact only after a successful build. Download from the latest successful run.
