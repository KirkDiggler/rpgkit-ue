# RPGKit UE — Architecture Guide

## What we built

A bridge that connects the [rpgkit](https://github.com/KirkDiggler/rpgkit) combat library
(Bus, Chain, Action, Effect) to Unreal Engine's Blueprint system. C++ owns the
combat logic; Blueprints own the UI and input.

Current milestone: the demo has a clickable UMG card HUD, card DataAssets,
multi-action cards, Vulnerable, Bleed, enemy attacks, block timing, combat log
text, and damage breakdown text. That means the next high-value work is code
organization, especially around actions and effects.

---

## The full picture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           UNREAL ENGINE                                  │
│                                                                          │
│  ┌─────────────────────┐      ┌──────────────────────────────────────┐  │
│  │   Level Blueprint    │      │        BP_RPGKitGameMode              │  │
│  │                     │      │                                      │  │
│  │  Keyboard Events    │      │  ┌──────────────────────────┐       │  │
│  │  (1, 2, 3, E)       │      │  │  BlueprintImplementable  │       │  │
│  │         │           │      │  │  Event overrides:        │       │  │
│  │  ┌──────▼──────┐    │      │  │                          │       │  │
│  │  │ Cast to     │    │      │  │  OnCombatLog(Message)    │       │  │
│  │  │ RPGKitGM    │────┼──────┼──│  OnDamageDealt(Result)    │       │  │
│  │  └──────┬──────┘    │      │  │  OnTurnEnded(Num)         │       │  │
│  │         │ calls     │      │  └──────────────────────────┘       │  │
│  │  ┌──────▼──────┐    │      │              ▲                       │  │
│  │  │ Strike()    │    │      │              │ C++ fires these       │  │
│  │  │ SetupEnc()  │    │      │              │ events                │  │
│  │  │ EndTurn()   │    │      │              │                       │  │
│  │  └─────────────┘    │      └──────────────┼───────────────────────┘  │
│  └─────────────────────┘                     │                          │
│                                              │                          │
│  ┌───────────────────────────────────────────┼──────────────────────┐  │
│  │               ARPGKitGameMode (C++)       │                      │  │
│  │                                           │                      │  │
│  │  ┌─────────────────────────────────────┐  │                      │  │
│  │  │ BlueprintCallable functions         │  │                      │  │
│  │  │  Strike(attacker, target, dmg)      │  │                      │  │
│  │  │  SetupEncounter(hero, enemy)         │  │                      │  │
│  │  │  EndTurn()                          │  │                      │  │
│  │  │  ApplyEffect(effect)                │  │                      │  │
│  │  └────────────────┬────────────────────┘  │                      │  │
│  │                   │                       │                      │  │
│  │        ┌──────────▼──────────┐           │                      │  │
│  │        │  URPGKitBus (C++)   │           │                      │  │
│  │        │                     │           │                      │  │
│  │        │  ExecuteDamageChain │           │                      │  │
│  │        │  ApplyEffect        │           │                      │  │
│  │        │  GetRawBus()        │           │                      │  │
│  │        └──────────┬─────────┘           │                      │  │
│  │                   │                     │                      │  │
│  │        ┌──────────▼──────────┐          │                      │  │
│  │        │   rpg::core::Bus    │          │                      │  │
│  │        │   (rpgkit library)  │          │                      │  │
│  │        └──────────┬──────────┘          │                      │  │
│  │                   │                     │                      │  │
│  │        ┌──────────▼──────────┐          │                      │  │
│  │        │  rpg::core::Chain   │          │                      │  │
│  │        │  → execute(event)   │          │                      │  │
│  │        │  → returns receipt  │          │                      │  │
│  │        └─────────────────────┘          │                      │  │
│  └─────────────────────────────────────────┘                      │  │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## The three layers

### Layer 1: rpgkit (pure C++20, no Unreal)

Files: `ThirdParty/rpgkit/core/include/rpg/core/*.hpp`

| Component | What it is | Key operation |
|-----------|-----------|---------------|
| `Bus` | Type-erased event router | subscribe(payload) → routes to handlers |
| `Chain<T>` | Per-resolution modifier pipeline | add(stage, id, modifier) → execute(value) → Result+receipt |
| `Action<T>` | Transient verb (strike, heal) | canActivate → activate (gate, spend, publish) |
| `Effect` | Persistent listener (bleed, rage) | apply → subscribes; remove → unsubscribes |
| `TopicDef<T>` | Static topic definition | on(bus) → bound Topic<T> |

The key insight: **publish collects, execute applies**. Subscribers add modifiers
to a chain during publish; transformation happens at execute(). This is what lets
Bless re-roll its 1d4 per attack.

### Layer 2: C++ wrappers (Unreal types)

Files: `Source/RPGKitUE/RPGKitBus.{h,cpp}`, `RPGKitGameMode.{h,cpp}`,
`RPGKitEncounterRuntime.{h,cpp}`, `RPGKitEffect.{h,cpp}`

| Unreal class | Wraps | Purpose |
|---|---|---|
| `URPGKitBus` | `rpg::core::Bus` | GameInstanceSubsystem — one bus per session |
| `ARPGKitGameMode` | Unreal bridge | Blueprint-facing API, card/deck flow, level/HUD integration |
| `URPGKitEncounterRuntime` | Encounter state | Fighters, request handlers, damage/block mutation, effect ownership |
| `URPGKitEffect` | `rpg::core::Effect` | Base class for Blueprintable effects |
| `URPGKitToughSkinEffect` | Tough Skin rule | Reduces damage against a specific entity |
| `URPGKitBleedEffect` | Bleed DoT | Stacks, ticks on turn.ended, self-removes at 0 |

**Two kinds of functions in ARPGKitGameMode:**

| Macro | Direction | Example |
|-------|-----------|---------|
| `UFUNCTION(BlueprintCallable)` | Blueprint → C++ | Strike(), EndTurn(), SetupEncounter() |
| `UFUNCTION(BlueprintImplementableEvent)` | C++ → Blueprint | OnCombatLog(), OnDamageDealt(), OnTurnEnded() |

BlueprintCallable = you call it FROM Blueprints.
BlueprintImplementableEvent = C++ fires it, YOU write the Blueprint body.

### Layer 3: Blueprints (editor-only, no C++ compile)

| Asset | What it is |
|-------|-----------|
| `BP_RPGKitGameMode` | Blueprint subclass of `ARPGKitGameMode`. Overrides events for UI. |
| `Level Blueprint` | Script attached to one level. Captures keyboard input, calls GameMode functions. |

---

## The flow: pressing "1" to strike

```
Player presses "1"
        │
        ▼
┌──────────────────────────────┐
│ Level Blueprint              │
│ Keyboard Event [1] Pressed   │
│   → Cast To RPGKitGameMode   │  ◀── this finds the current GameMode object
│   → Strike("hero","goblin",5)│  ◀── calls C++ function
└──────────────┬───────────────┘
               │
        ┌──────▼──────────────────────────────────────────┐
        │ ARPGKitGameMode::Strike()                       │
        │                                                 │
        │  1. Build FRPGKitDamageEvent{hero,goblin,5}     │
        │  2. Call BusSubsystem->ExecuteDamageChain()     │
        │         │                                       │
        │         ├─ Creates rpg::core::Chain<DamageEvent>│
        │         ├─ Publishes to "combat.damage" topic   │
        │         │   (subscribers add modifiers)          │
        │         ├─ Executes the chain (folds modifiers) │
        │         └─ Returns FRPGKitChainResult           │
        │                                                 │
        │  3. Applies final damage to Target fighter      │
        │     (deducts block first, then HP)              │
        │                                                 │
        │  4. Fires Blueprint events:                      │
        │     → OnDamageDealt(Result)                      │
        │     → OnCombatLog("hero strikes goblin...")      │
        └─────────────────────────────────────────────────┘
               │
        ┌──────▼──────────────────────────────────────────┐
        │ BP_RPGKitGameMode (your Blueprint overrides)    │
        │                                                 │
        │  OnCombatLog(Message) → Print String(Message)   │
        │  OnDamageDealt(Result) → Print String(Value)     │
        └─────────────────────────────────────────────────┘
               │
               ▼
        [Text appears in Output Log / on screen]
```

---

## What each Blueprint node means

### In the Level Blueprint:

```
┌──────────────────────────┐
│  Keyboard Event [1]      │  ← "When player presses 1"
│      Pressed ▶───────────│── goes to next node
└──────────────────────────┘
            │
┌───────────▼──────────────┐
│  Cast To RPGKitGameMode  │  ← "Give me the GameMode as our C++ type"
│      As RPGKit Game Mode │── the reference to call functions on
└──────────────────────────┘
            │
┌───────────▼──────────────┐
│  Strike                  │  ← "Call the Strike function"
│   Target: self           │     (self = the GameMode we just cast to)
│   AttackerId: hero       │
│   TargetId: goblin       │
│   BaseDamage: 5          │
└──────────────────────────┘
```

**Why Cast?** `Get Game Mode` returns a generic `AGameModeBase*`. Cast converts it
to `ARPGKitGameMode*` so you can call our specific functions like `Strike()`.

### In BP_RPGKitGameMode:

```
┌──────────────────────────────────────────┐
│  Event OnCombatLog                       │  ← "C++ fired this event"
│      Message ▶───────────────────────────│── the text string
└──────────────────────────────────────────┘
            │
┌───────────▼──────────────┐
│  Print String             │  ← "Show this text"
│   In String: [Message]   │
└──────────────────────────┘
```

No Cast needed here — you're already INSIDE the BP_RPGKitGameMode.

---

## The variable step (what we skipped)

You asked about the "Get GameMode" and variable step I mentioned earlier.

The original idea was:
1. Cast once on BeginPlay
2. Store the result in a variable (`GameModeRef`)
3. Each key event reads the variable instead of casting again

What we actually did (simpler):
1. Each key event does its own Cast — independent chains

Both work. The variable version is slightly cleaner when you have many events.
Here's what it looks like:

```
[BeginPlay] → [Cast To RPGKitGameMode] → [Set GameModeRef]

[Key 1] → [Get GameModeRef] → [Strike ...]
[Key 2] → [Get GameModeRef] → [Strike ...]
```

`Set` stores the reference. `Get` reads it back. Only cast once.

---

## What each C++ file does at a glance

| File | Role | Lines |
|------|------|-------|
| `RPGKitGameMode.h` | Declares Fighter struct, Card struct, GameMode class with all BlueprintCallable + BlueprintImplementableEvent functions | 169 |
| `RPGKitGameMode.cpp` | Implements SetupEncounter, Strike, EndTurn — the combat orchestration | 192 |
| `RPGKitBus.h` | Declares damage event struct, chain result struct, topic definitions, Bus subsystem | 125 |
| `RPGKitBus.cpp` | Creates rpg::core::Bus, implements ExecuteDamageChain (build chain, publish, execute, convert results) | 93 |
| `RPGKitEffect.h` | Declares base Effect + ToughSkin + Bleed Blueprintable classes | ~100 |
| `RPGKitEffect.cpp` | Inner rpg::core::Effect subclasses that subscribe to Bus topics | ~120 |

---

## Where to go from here

The system is working. Next steps:

1. **Extract action execution** — move `ExecuteCardAction`/target resolution out of `ARPGKitGameMode` behind a small executor.
2. **Separate effect definitions from effect instances** — author effect data, then create runtime `URPGKitEffect` objects when actions resolve.
3. **Replace overloaded action fields** — stop using `Amount` and `DurationTurns` for every action meaning once the next effect proves the need.
4. **Move enemy intent to data** — replace the hardcoded goblin attack with an authored enemy intent/action list.

---

## C++ Organization Notes For The Next Refactor

This section is intentionally practical. The goal is not to write "perfect C++."
The goal is to keep the Unreal adapter readable while moving rules out of the
GameMode.

### C++ Mental Model Coming From Go

Go tends to organize behavior around packages and small interfaces. C++ in Unreal
organizes behavior around reflected types and engine-managed object lifetimes.

Useful mapping:

| Go habit | Unreal C++ equivalent |
|---|---|
| package-level structs | `USTRUCT` in a module header |
| interface for behavior | abstract C++ class or narrow runtime context object |
| plain value config | `USTRUCT(BlueprintType)` or `UDataAsset` |
| long-lived service | `UObject`, `UGameInstanceSubsystem`, or owned C++ member |
| map/slice | `TMap`, `TArray` |
| pointer owned by runtime | `TObjectPtr<UObjectType>` when Unreal owns/tracks it |

Unreal reflection changes normal C++ design:

```text
If Blueprint/editor must see it, use USTRUCT/UCLASS/UENUM/UFUNCTION/UPROPERTY.
If only C++ needs it, keep it as plain C++.
If Unreal owns a UObject pointer, expose/hold it with UPROPERTY/TObjectPtr.
If it is short-lived helper logic, prefer plain C++ over UObject.
```

### What Should Move Out Of GameMode

`ARPGKitGameMode` should remain the demo's encounter host for now. It can own:

```text
fighter map
current hand
energy
turn number
active effects
bus subsystem pointer
Blueprint entry points
```

It should not keep growing forever with:

```text
every action switch case
every target rule
every concrete effect creation rule
enemy intent rules
combat-log formatting rules
```

The next extraction target is action execution because both player cards and the
enemy attack already share that path.

### Recommended Minimal File Shape

Start with a small split, not a large framework:

```text
RPGKitActionTypes.h
  ERPGKitCardActionType
  ERPGKitActionTargetMode
  FRPGKitCardAction
  FRPGKitActionContext

RPGKitActionExecutor.h/.cpp
  FRPGKitActionExecutor
  ExecuteAction(...)
  ResolveActionTargetId(...)

RPGKitGameMode.h/.cpp
  owns state and Blueprint API
  calls ActionExecutor

RPGKitEffect.h/.cpp
  runtime effect UObjects
```

Use a plain C++ struct/class for `FRPGKitActionExecutor` at first. It does not
need to be a `UObject` unless Blueprint must instantiate it or Unreal must manage
its lifetime.

### The Runtime Context Problem

An action executor needs to mutate encounter state without becoming GameMode by
another name.

For the first extraction, it is acceptable for the executor to receive a pointer
or reference to `ARPGKitGameMode` and call existing methods:

```cpp
Executor.ExecuteAction(*this, Context, Action);
```

That is not the final architecture, but it is a safe move-only refactor. After it
works, identify the small surface the executor actually needs:

```text
GetFighter
FindFighter or ApplyDamage/Heal/AddBlock
Strike
DealRawDamage
ApplyEffect
EmitCombatLog or observer hook
```

Then consider a narrow runtime interface or context struct.

Do not start by inventing an abstract interface unless the duplicated dependency
is obvious. In Unreal C++, premature abstraction adds header complexity quickly.

### Effect Definitions vs Effect Instances

The current runtime effects are concrete `UObject`s:

```text
URPGKitBleedEffect
URPGKitVulnerableEffect
```

They are instances because they subscribe to the bus, tick down, and eventually
remove themselves.

The authored content should become definitions:

```text
Bleed definition: stacks, damage per stack
Vulnerable definition: percent bonus, turns
```

Good next step:

```text
FRPGKitEffectSpec
  EffectType
  Magnitude
  Duration
  SecondaryMagnitude
```

Then `ApplyEffect` actions can carry an effect spec instead of overloading
`Amount` and `DurationTurns` forever.

### When To Add New Types

Add a new type when one of these is true:

```text
the same concept is used by both player cards and enemy intents
a field name is lying, such as DurationTurns meaning damage per stack
Blueprint/DataAsset authors need to edit the concept directly
the GameMode switch gains another unrelated responsibility
tests or logs need to name the concept clearly
```

Avoid new types when:

```text
there is only one caller
the type would only forward to one method
the name is speculative
we have not observed the second use case yet
```

### Safe Refactor Order

Use this order to keep the project buildable:

1. Move enums/structs only if needed; build after reflected header changes.
2. Extract `ResolveActionTargetId` unchanged; build.
3. Extract `ExecuteCardAction` unchanged; build.
4. Rename it to `ExecuteAction` only after call sites are stable.
5. Add an effect spec only when creating the next effect/action proves the current fields are wrong.
6. Move enemy attack content to an authored definition after player action execution is stable.

Reflection rule: after adding/removing `USTRUCT`, `UENUM`, `UCLASS`, `UFUNCTION`,
or `UPROPERTY`, prefer a full editor restart/build. For function-body-only edits,
Live Coding is usually fine.
