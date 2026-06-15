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
  encounter state, card model, action interpreter, Blueprint API

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

## Next Step

Extract action execution out of `ARPGKitGameMode` with minimal behavior change.

Recommended order:

1. Create `RPGKitActionExecutor.h/.cpp`.
2. Move `ResolveActionTargetId` unchanged.
3. Move `ExecuteCardAction` unchanged.
4. Keep `ARPGKitGameMode` as the runtime owner and Blueprint-facing API.
5. Build and verify the existing demo loop still works.

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

Full C++ build succeeded after the card text formatting change:

```text
RPGKitUEEditor Win64 Development -NoHotReload
Result: Succeeded
```
