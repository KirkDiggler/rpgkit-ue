# Observability

This folder captures the observability use case discovered while building the
Unreal workshop demo.

The goal is to make a strong, concrete case for what `rpgkit` should expose so
hosts can inspect, debug, explain, and visualize combat resolution.

## Documents

| Doc | Purpose |
|---|---|
| `use-case.md` | The concrete Unreal demo scenario and what a developer needs to see |
| `design.md` | A first-pass design sketch for observable events/hooks |
| `status.md` | Current state, decisions, open questions, next steps |
| `AGENTS.md` | Guidance for future agents working on these docs |

## Core Idea

Combat logs are output. Observability is explanation.

The combat log may say:

```text
Dirty Gobbo takes 8 damage.
```

Observability should explain:

```text
Strike started with 5 base damage.
combat.damage was published.
Vulnerable subscriber ran.
Vulnerable added an effects-stage modifier.
The chain executed vulnerable-goblin: 5 -> 8.
GameMode applied 8 final damage to Dirty Gobbo.
```

The Unreal HUD can consume either combat log lines or raw observation records,
but `rpgkit` should provide enough structured information to derive both.
