# Observability Status

## Current State

The Unreal workshop demo has host-side visibility:

```text
manual combat log strings
recent combat log stored on GameMode
debug HUD renders state and logs
latest damage chain breakdown rendered on HUD
```

The demo proves the need for deeper structured observations but does not yet have
a generic observability API.

## Implemented In rpgkit-ue

```text
EmitCombatLog helper
RecentCombatLog storage
LatestDamageBreakdown storage
debug HUD display
raw damage request event flow for Bleed
```

## Not Yet Implemented

```text
core rpgkit observation sink
structured observation records
correlation ids
combat log formatter from observations
raw observation debug panel
effect subscription observations
topic publish/subscriber observations
```

## Current Learning

The most valuable observation points are at:

```text
effect apply/remove
effect subscription
topic publish
subscriber invoke
modifier add
chain execute
state mutation by host runtime
```

Manual logs are useful for the workshop but should become derived output later.

## Next Step

Use `use-case.md` to create a strong issue/design task in `rpgkit` for a minimum
observable core API.
