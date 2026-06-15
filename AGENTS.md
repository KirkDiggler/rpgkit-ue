# RPGKit UE Agent Notes

This repo is an Unreal workshop host for proving `rpgkit` mechanics in a visual,
designer-facing environment.

## Priorities

1. Keep C++ as the source of gameplay rules.
2. Keep Blueprints/UMG focused on input, display, and asset wiring.
3. Prefer small, buildable refactors over large architecture rewrites.
4. Update docs/status when changing workflow, architecture, or demo behavior.
5. Preserve the current visual demo loop while improving organization.

## Current Direction

The next major seam is action/effect organization:

```text
ARPGKitGameMode
  -> remains encounter host and Blueprint-facing API

ActionExecutor
  -> resolves targets
  -> executes action specs
  -> shared by player cards and enemy intents

Effect definitions/specs
  -> authored content
  -> create runtime URPGKitEffect instances
```

Start by moving existing behavior unchanged. Do not invent a broad framework
until the second concrete use case forces it.

## Build Notes

Full Windows build command:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" RPGKitUEEditor Win64 Development -Project="C:\Users\kirk\source\repos\rpgkit-ue\RPGKitUE.uproject" -WaitMutex -NoHotReload
```

If Live Coding is active, the full build may fail. Press `Ctrl+Alt+F11` in the
editor for function-body iteration or close Unreal before a full reflected build.

## Documentation Map

```text
docs/unreal-workshop-roadmap.md    current direction and implementation order
docs/architecture-guide.md         code-first architecture and C++ organization notes
docs/code-first-tour.md            walkthrough of the C++/Blueprint bridge
docs/blueprint-building-blocks.md  card/action/DataAsset setup
docs/umg-debug-hud-tutorial.md     UMG lessons from the current HUD
docs/observability/                future structured observability design
STATUS.md                          current handoff state
```

## Git/PR Workflow

Use branches and PRs for meaningful increments. Good PR size for this repo:

```text
one refactor seam
one mechanic
one documentation update
```

Before committing, inspect status and diff. Avoid committing generated Unreal
folders such as `Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache`.
