# GD AutoBot

A separate Geode mod project for Geometry Dash.

## Goals
- Independent in-game BOT ON/OFF control.
- No macro recording or replay playback.
- Adaptive decision policy and persistent per-level learning memory.
- Gradual support for Cube, Ship, Ball, UFO, Wave, Robot and Spider.

## Prototype status
The first scaffold provides an in-game toggle HUD and learning-data foundation. It does **not** yet autonomously solve levels. Obstacle sensing, movement prediction, input timing and per-mode policies must be implemented and tested before it can be called a playable autoplayer.

Target configured here: Android Geometry Dash 2.2081 with Geode 5.11.1.
