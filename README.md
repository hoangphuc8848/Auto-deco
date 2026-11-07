# GD AutoBot

Independent Geode mod project targeting Android Geometry Dash 2.2081 and Geode 5.11.1.

## Current implementation
- In-game BOT ON/OFF HUD prototype.
- Reactive controller scaffold for Cube/Robot and Wave observations.
- Persistent per-situation counters scaffold.

## Important status
This branch is still a development prototype, not a working autonomous player.
The HUD toggle does not yet drive the player. Live observation collection, hazard classification,
collision prediction, per-mode control policies, and in-game outcome feedback still need to be
implemented and tested on Android.

## Direction
The project uses ordinary reactive/adaptive control. Frame Perfect timing calibration was removed.
No completion-rate or Tidal Wave capability is claimed until measured in actual gameplay.
