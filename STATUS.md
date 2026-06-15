# RPGKit UE Status

## Current Milestone

The project has a working visual Unreal combat demo:

```text
Card DataAssets
clickable UMG card HUD
readable card face text from C++
energy spending
Damage / Block / Heal actions
Apply Vulnerable action
Apply Bleed action
Vulnerable modifies damage through the rpgkit chain
Bleed ticks on turn.ended through raw damage requests
enemy attacks on End Turn
block absorbs damage then expires after enemy attack
recent combat log text
latest damage breakdown text
```

## Current Code Shape

Important files:

```text
Source/RPGKitUE/RPGKitGameMode.h/.cpp
  encounter state, card model, Blueprint API

Source/RPGKitUE/RPGKitActionExecutor.h/.cpp
  plain C++ action executor shared by player cards and enemy attacks

Source/RPGKitUE/RPGKitBus.h/.cpp
  Unreal subsystem owning rpg::core::Bus and damage chain execution

Source/RPGKitUE/RPGKitEffect.h/.cpp
  runtime effects: Bleed, Vulnerable, Tough Skin

Content/Cards/
  card DataAssets used by the demo

Content/Blueprints/WBP_CombatHUD_Simple.uasset
  current clickable HUD

Content/Blueprints/BP_RPGKitGameMode.uasset
  GameMode Blueprint wiring setup/hand/HUD creation

Content/Tutorial-1.umap
  current working map
```

## Current Branch

Branch: `refactor/action-executor`

This branch extracts action execution out of `ARPGKitGameMode` with minimal
behavior change.

Implemented:

1. Created `RPGKitActionExecutor.h/.cpp`.
2. Moved target resolution into `FRPGKitActionExecutor`.
3. Moved action dispatch into `FRPGKitActionExecutor::ExecuteAction`.
4. Kept `ARPGKitGameMode` as the runtime owner and Blueprint-facing API.

Still required before merge:

1. Verify the existing demo loop still works in Unreal.

Do not start by redesigning effect specs or enemy intent data. Those are next
after action execution is stable.

## Known Follow-Ups

```text
FRPGKitCardAction.Amount and DurationTurns are overloaded.
Enemy attack is hardcoded in EnemyTakeTurn.
Effects are runtime objects but not yet backed by clean authored definitions.
Combat logs are manual strings, not structured observations.
Git LFS is not currently configured; current binary assets are small.
```

## Verified Recently

Full C++ build succeeded after extracting `FRPGKitActionExecutor`:

```text
RPGKitUEEditor Win64 Development -NoHotReload
Result: Succeeded
```
