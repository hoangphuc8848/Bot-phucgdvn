# AI Auto Play — Ultimate Menu branch

Android64 Geode mod targeting Geometry Dash 2.2.081 and Geode 5.11.1.

## Implemented in this branch
- Screen-fixed in-game control strip: BOT ON/OFF, manual JUMP, RELEASE.
- Reaction timing adjustment with R- / R+ and a new R0 reset control.
- Offline Cube hazard heuristic; no API key, cloud model, or network dependency.
- GitHub Actions Android64 build workflow is enabled for this branch.

## Honest limitations
This is an incremental upgrade of the existing prototype, not an all-in-one clone of every Geode mod. The bot is heuristic and Cube-focused; it is not frame-perfect and does not guarantee completion. Ship, Wave, Ball, Robot, Spider and Swing automation are not implemented. Third-party mod features can only be integrated when their source/license and APIs permit it.

## Build
Pushes to `main` and `ultimate-menu-v1` run the Android64 workflow. Download the `.geode` artifact from the successful Actions run.
