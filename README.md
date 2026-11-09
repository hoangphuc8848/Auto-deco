# GD AutoBot / Offline Level Assistant

Android64 Geode development project.

## Implemented in source
- Offline keyword-based assistant parser: Vietnamese/English intents and themes; no API, network call, or API key.
- Auto Deco planning core: local palettes and decoration-layer plans for tech, hell, space, forest, and Nine Circles.
- Gameplay Builder core: deterministic section generator with configurable difficulty, length, and optional wave corridor.
- Existing experimental Cube-only gameplay HUD prototype and persistent situation-statistics scaffold.

## Current limitations
The three new modules are planning engines, not a finished in-editor assistant. They return structured plans; they do not yet place/edit Geometry Dash objects, show a chat panel, or generate a playable level automatically. Those require editor integration and Android testing. The keyword parser is limited and is not a general-purpose LLM.

## Offline/privacy
The assistant, decoration, and gameplay planning code is deterministic local C++; it makes no AI API requests and needs no API key.

## Build
GitHub Actions builds the Android64 .geode artifact on branch gd-autobot. Download it from the latest successful workflow run.
