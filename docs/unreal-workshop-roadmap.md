# RPGKit Unreal Workshop Roadmap

This project is not primarily about making an Unreal game first.

It is a workshop surface for proving that `rpgkit` can be useful to someone
experimenting with, designing, debugging, and teaching RPG combat systems.

The terminal demo proves the code-first path. The Unreal demo should prove the
tool-builder and designer-facing path.

## North Star

```text
rpgkit provides portable combat mechanics.
rpgkit-ue exposes those mechanics as Unreal-visible tools.
Blueprints and DataAssets author content and visualize runtime behavior.
Observability explains what happened inside the toolkit.
```

The goal is an Across-the-Obelisk-style capability surface:

```text
cards
buffs
debuffs
items
class abilities
enemy intents
target selection
chain breakdowns
combat logs
debug/inspection tools
```

The immediate goal is smaller:

```text
make RPGKit mechanics visible, authorable, and inspectable in Unreal.
```

## Relationship To Other Repos

| Repo | Role |
|---|---|
| `rpgkit` | Portable C++ core and tutorial source of truth |
| `rpgkit-demo-game` | Terminal implementation proving docs/API for code-first users |
| `rpg-toolkit` | Go design journey, rulebook exploration, architecture lessons |
| `rpgkit-ue` | Unreal workshop host for visual tools and designer-facing workflows |

`rpgkit-ue` should not invent a separate rules architecture. It should pressure
test `rpgkit` and reveal missing toolkit seams.

## Current Baseline

The Unreal demo currently has:

```text
CardDefinition DataAssets
Card actions: Damage, Block, Heal, Apply Bleed, Apply Vulnerable
Target modes: Self, Enemy, Explicit
GameMode card interpreter
Shared action executor seam
Damage through rpgkit Chain/Bus
Vulnerable modifying the damage chain
Bleed ticking from turn.ended through raw damage requests
Block as direct fighter state
Heal as direct fighter state
EndTurn block expiration
UMG card-button HUD
Formatted card text from C++
Recent combat log on HUD
Latest damage breakdown helpers
Blueprint input for card play and end turn
Hardcoded enemy attack on end turn
```

The key working path is:

```text
DataAsset card
  -> BP_RPGKitGameMode.CardDefinitions
  -> ARPGKitGameMode::DealHand
  -> CurrentHand
  -> DebugPlayCard / PlayCard
  -> Resolve target mode
  -> ExecuteCardAction
  -> rpgkit Bus/Chain when needed
  -> HUD + Blueprint combat log
```

`ExecuteCardAction` is still hosted by `ARPGKitGameMode`, but it is the first
organization seam for a future action executor. Player cards and the temporary
goblin attack both use this path.

## Current Milestone

The current milestone is a working visual combat loop:

```text
author cards as DataAssets
deal a hand
click card buttons in UMG
spend energy
resolve card actions
show readable card faces
apply Vulnerable and Bleed
end turn
tick effects
enemy attacks
block absorbs damage then expires
deal a new hand
show combat log and damage breakdown text
```

This is enough proof that `rpgkit-ue` can make mechanics authorable and visible.
The next work should be organization, not more one-off mechanics.

## Next Focus: Actions And Effects Organization

The exciting next seam is breaking action and effect logic out of
`ARPGKitGameMode` without losing the simple mental model.

Current pressure points:

```text
ARPGKitGameMode still owns card/deck flow and Blueprint API
URPGKitEncounterRuntime owns encounter state and request handlers
FRPGKitActionExecutor still creates concrete effects directly
FRPGKitCardAction uses generic Amount and DurationTurns for too many meanings
enemy attacks reuse the card action path, but still live as hardcoded content
```

Good next shape:

```text
CardDefinition DataAsset
  -> array of ActionSpecs

ActionExecutor
  -> resolves targets
  -> dispatches action kinds
  -> publishes typed requests where possible

EffectFactory or EffectDefinition
  -> turns authored effect data into runtime URPGKitEffect objects

EncounterRuntime
  -> owns encounter state
  -> handles typed requests
  -> mutates combat state

ARPGKitGameMode
  -> bridges Unreal/Blueprint to the runtime
  -> remains the Blueprint-facing entry point
```

Important: do this incrementally. The first refactor should move code, not invent
a new game architecture.

## Tool Boundary Rules

Use these to decide where code belongs.

### Definitions vs Instances

Definitions are authored content:

```text
CardDefinition
ActionSpec
EffectDefinition
ItemDefinition
ClassDefinition
EnemyDefinition
EncounterDefinition
```

Instances are runtime state:

```text
CurrentHand
FighterState
ActiveEffects
TurnState
SelectedTargets
EncounterRuntime
```

Do not mix these unless we are deliberately prototyping a seam.

### Cards Declare Intent

Cards should not implement mechanics.

Good:

```text
Strike
  Cost: 1
  Actions:
    Damage 5 -> Enemy
```

Bad:

```text
if CardName == "Strike" then goblin.CurrentHP -= 5
```

The GameMode or future Encounter runtime interprets the card's declared actions.

### Effects Subscribe

Persistent behavior belongs in effects:

```text
Tough Skin modifies incoming damage.
Vulnerable modifies damage taken.
Bleed reacts to turn ended.
Rage modifies outgoing damage.
```

Effects subscribe to bus topics and remove their subscriptions when they expire.

### Events Describe What Happened

Do not use events to carry entire gameplay objects when a thin payload describes
the change.

Good:

```text
EffectApplied { target, effect id, stacks, duration }
ActionResolved { actor, action id, target, result }
ModifierAdded { chain id, source, stage }
```

Bad:

```text
Event carries full mutable Effect object when listeners only need type/data.
```

This follows the `rpg-toolkit` journey insight: events carry facts about actions
and changes, not object ownership.

### Hardcoding Rule

If gameplay content is hardcoded, pause and evaluate the tradeoff.

Allowed temporarily:

```text
hero/goblin demo ids inside PlayCard
debug key bindings
debug HUD layout
simple enemy AI while discovering seams
```

Should become tools:

```text
targeting
actions
effects
items
enemy intents
encounters
class abilities
status duration
combat log generation
```

## Observability Direction

Manual combat log strings are useful for the prototype, but they are not the
long-term tool.

The long-term direction is structured observability from inside `rpgkit` and the
host adapter.

Developers need to answer:

```text
What action started?
What targets were selected?
What topic was published?
Which subscribers ran?
Which effect added which modifier?
What did the chain do at each stage?
Why was final damage different from base damage?
When did an effect apply, tick, expire, or remove?
What resource was spent?
What runtime state changed?
```

Potential hook vocabulary:

```text
ActionStarted
ActionCanActivateChecked
ActionActivated
ActionResolved

TargetResolved
ResourceSpent
ResourceChanged

TopicPublished
SubscriberInvoked
SubscriberFailed

ChainCreated
ModifierAdded
ChainExecuted
ChainStepResolved

EffectApplied
EffectTicked
EffectExpired
EffectRemoved

StateChanged
CombatLogLineProduced
```

The Unreal debug HUD should eventually consume structured observations instead
of bespoke `EmitCombatLog` strings.

Prototype path:

```text
GameMode emits strings
  -> recent HUD log

structured observations in rpgkit/rpgkit-ue
  -> observer converts to combat log lines
  -> HUD shows generated lines
  -> debug panel can inspect raw events
```

## Workshop Tracks

### Workshop 1: Effects As First-Class Mechanics

Build a real debuff such as `Vulnerable`.

Desired behavior:

```text
Vulnerable target takes +50% damage for N turns.
```

This reveals:

```text
effect definitions
effect instances
duration
turn ticking
card action applies effect
damage chain modifier
effect lifecycle observability
```

Minimal implementation:

```text
URPGKitVulnerableEffect
  TargetEntityId
  PercentBonus
  RemainingTurns

onApply:
  subscribe combat.damage chained topic
  subscribe turn.ended topic
```

Card:

```text
Expose Weakness
  Cost: 1
  Actions:
    Apply Vulnerable +50% for 2 turns -> Enemy
```

Likely new seam:

```text
Card actions need to apply effect definitions, not just Damage/Block/Heal.
```

### Workshop 2: Enemy Intents

Build the first enemy turn loop.

Desired behavior:

```text
Player plays cards.
Player ends turn.
Enemy intent resolves.
New hand is dealt.
```

This reveals:

```text
enemy action definitions
intent preview
turn ownership
encounter runtime
```

Current first pass:

```text
EndTurn
  -> publish turn.ended
  -> EnemyTakeTurn
      -> hardcoded goblin Damage 6 action
      -> ExecuteCardAction with actor goblin / enemy hero
  -> ClearAllBlock
  -> DealHand(5)
```

The hardcoded attack is intentional scaffolding. The next seam is
`EnemyDefinition` / `IntentDefinition` data.

Current timing rule: block lasts until round cleanup, after the enemy acts.
Future effects such as "retain block" can hook into this timing once cleanup is
made observable.

### Workshop 3: Target Selection

Move past demo `Self`/`Enemy` targeting.

Desired behavior:

```text
single enemy
all enemies
self
ally
random enemy
selected target
```

This reveals:

```text
selector mechanics
target validation
UI target picking
multi-target action resolution
```

### Workshop 4: Combat Log From Observability

Replace hand-written gameplay strings with structured observations.

Desired behavior:

```text
Damage chain runs.
Each modifier is observed.
Combat log line is generated from structured data.
HUD can show both summary and breakdown.
```

This reveals:

```text
core hook API
adapter observer API
combat log formatter
debug inspector
```

## Near-Term Implementation Order

1. Keep current debug HUD and card action baseline stable.
2. Implement `Vulnerable` as a real effect workshop.
3. Add `ApplyEffect` or `ApplyStatus` card action support.
4. Add enough effect lifecycle logging to understand what happened.
5. Decide what belongs in `rpgkit` core vs `rpgkit-ue` adapter.

## Project Board Groupings

The GitHub Project should stay grouped around outcomes, not every tiny code step.

### Epic: Playable Combat Scenario

Goal: a visual, playable RPGKit combat loop in Unreal.

Includes:

```text
authored cards
player hand
enemy turn/intents
block/turn flow
new hand/energy reset
basic win/loss feedback
debug HUD visibility
```

### Epic: Effects And Statuses Workshop

Goal: prove buffs/debuffs/statuses as first-class rpgkit mechanics in Unreal.

Includes:

```text
Vulnerable
Bleed via raw damage request
future resistance/weak/strength-like effects
duration ticking
stack behavior
expiration
card actions that apply effects
```

### Epic: Action Execution Layer

Goal: extract a shared action execution path used by player cards and enemy
intents.

Includes:

```text
ActionContext
ExecuteCardAction / ExecuteAction
target resolution
direct state actions
effect application actions
reducing ARPGKitGameMode bloat
```

### Epic: Observability And Combat Explanation

Goal: make rpgkit internals visible enough to debug and build combat logs.

Includes:

```text
chain breakdown HUD
raw damage request visibility
action/effect lifecycle events
structured hook vocabulary
manual log strings replaced by observations
```

### Epic: Unreal Authoring Tools And Organization

Goal: organize `rpgkit-ue` into designer-facing building blocks.

Includes:

```text
folders/classes for actions
effects
card definitions
enemy definitions
debug HUD
workshop docs
future Blueprint/UMG surfaces
```

## Done Means

A workshop step is done when:

```text
the mechanic is authorable in Unreal content
the runtime path is visible in code
the result appears in the debug HUD
the implementation reveals whether a toolkit seam is missing
the docs explain the code-first mental model
```
