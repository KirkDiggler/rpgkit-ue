# RPGKit UE

Unreal Engine workshop host for proving and visualizing
[`rpgkit`](https://github.com/KirkDiggler/rpgkit) combat mechanics.

This repo is intentionally code-first:

```text
C++ owns combat rules and runtime state.
DataAssets author cards and content.
Blueprints/UMG handle input, display, and editor wiring.
```

## Current Demo

The current milestone is a small playable combat loop:

```text
clickable card HUD
card DataAssets
Damage / Block / Heal actions
Vulnerable damage modifier
Bleed turn tick
enemy attack on End Turn
block timing
combat log and damage breakdown text
```

See `STATUS.md` for the latest handoff state.

## Useful Docs

```text
AGENTS.md                          repo instructions for future agent sessions
STATUS.md                          current project state and next step
docs/unreal-workshop-roadmap.md    roadmap and implementation order
docs/architecture-guide.md         C++/Unreal architecture notes
docs/code-first-tour.md            code-first walkthrough
docs/blueprint-building-blocks.md  DataAsset and Blueprint building blocks
docs/umg-debug-hud-tutorial.md     UMG HUD notes
docs/observability/                future observability design
```

## Build

On Windows with Unreal Engine 5.7 installed:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" RPGKitUEEditor Win64 Development -Project="C:\Users\kirk\source\repos\rpgkit-ue\RPGKitUE.uproject" -WaitMutex -NoHotReload
```

If Live Coding is active, press `Ctrl+Alt+F11` in the editor or close Unreal
before running a full build.
