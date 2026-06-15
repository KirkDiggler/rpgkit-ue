# Observability Use Case: Explain A Combat Round

## Context

The Unreal workshop demo now has a small but meaningful combat loop:

```text
Stanthony vs Dirty Gobbo

Player cards:
  Strike           -> Damage 5 to Enemy
  Defend           -> Block 4/5 to Self
  Shield Bash      -> Block 4 to Self, Damage 4 to Enemy
  Expose Weakness  -> Apply Vulnerable +50% for 2 turns to Enemy
  Rend             -> Apply Bleed stacks to Enemy

Enemy:
  Dirty Gobbo attacks for 6 damage at end of player turn.
```

Mechanics currently represented:

```text
CardDefinition DataAssets
Action specs
Self/Enemy target resolution
Damage chain
Persistent effects
Turn-ended ticking
Raw damage request event
Block cleanup at round end
Debug HUD
Recent combat log
Latest chain breakdown
```

This is enough to define a useful observability problem.

## The Scenario

The player performs:

```text
1. Play Expose Weakness on Dirty Gobbo.
2. Play Strike on Dirty Gobbo.
3. Play Rend on Dirty Gobbo.
4. Play Defend or Shield Bash to gain block.
5. End turn.
```

Expected gameplay result:

```text
Dirty Gobbo becomes Vulnerable.
Strike deals 8 instead of 5 because Vulnerable adds +50% damage.
Dirty Gobbo starts Bleeding.
At turn end, Bleed requests raw damage and Dirty Gobbo loses HP.
Dirty Gobbo attacks Stanthony.
Stanthony's block absorbs some damage.
Block expires at round cleanup.
A new hand is dealt.
```

## What The Developer Wants To Know

When the result is correct, the developer wants to explain it.

When the result is wrong, the developer wants to locate the failure.

Questions:

```text
What action started?
What card or source caused it?
How was the target resolved?
What topic was published?
Which subscribers ran?
Which effect added which modifier?
What did the chain look like before execution?
What did each chain step do?
What final value was produced?
Who applied the resulting state mutation?
Did an effect tick?
Did an effect expire?
Did a nested publish happen?
Was raw damage requested?
Who handled the request?
What state changed?
```

## Current Manual Output

The current Unreal demo manually logs strings such as:

```text
Playing [1] Expose Weakness.
Dirty Gobbo is Vulnerable (+50% damage) for 2 turns.
Playing [2] Strike.
hero strikes goblin for 8 damage -> Dirty Gobbo HP: 12/20
Playing [3] Rend.
Dirty Gobbo is Bleeding (3 stacks, 2 damage per stack).
--- Turn 1 ---
bleed requests 6 raw damage to goblin.
Dirty Gobbo takes 6 raw damage -> HP: 6/20
Dirty Gobbo attacks!
Stanthony blocks 4 damage! (2 gets through)
goblin strikes hero for 6 damage -> Stanthony HP: 28/30
Stanthony's block expires.
```

The debug HUD also shows the latest damage chain breakdown, for example:

```text
Latest Damage Chain:
  effects / vulnerable-goblin: 5 -> 8
```

This is useful, but it is handcrafted by the Unreal host.

## Desired Observable Trace

The same scenario should be explainable through structured observations.

Example trace shape:

```text
ActionStarted
  source: Expose Weakness
  actor: hero

TargetResolved
  mode: Enemy
  result: goblin

EffectApplied
  effect: vulnerable-goblin
  target: goblin
  source: Expose Weakness
  duration: 2

EffectSubscribed
  effect: vulnerable-goblin
  topic: combat.damage
  flavor: chained

EffectSubscribed
  effect: vulnerable-goblin
  topic: turn.ended
  flavor: notification

ActionStarted
  source: Strike
  actor: hero

TargetResolved
  mode: Enemy
  result: goblin

TopicPublished
  topic: combat.damage
  flavor: chained
  payload: { attacker: hero, target: goblin, baseAmount: 5 }

SubscriberInvoked
  topic: combat.damage
  subscriber: vulnerable-goblin

ModifierAdded
  chain: combat.damage
  stage: effects
  modifier: vulnerable-goblin

ChainExecuted
  chain: combat.damage
  initial: 5
  final: 8

ChainStepResolved
  stage: effects
  modifier: vulnerable-goblin
  before: 5
  after: 8

StateChanged
  entity: goblin
  field: hp
  before: 20
  after: 12
  source: Strike

ActionStarted
  source: Rend
  actor: hero

EffectApplied
  effect: bleed-goblin
  target: goblin
  stacks: 3
  damagePerStack: 2

TopicPublished
  topic: turn.ended
  payload: 1

SubscriberInvoked
  topic: turn.ended
  subscriber: bleed-goblin

EffectTicked
  effect: bleed-goblin
  target: goblin
  amount: 6
  remainingStacks: 2

TopicPublished
  topic: combat.raw_damage.requested
  payload: { source: bleed, target: goblin, amount: 6 }

SubscriberInvoked
  topic: combat.raw_damage.requested
  subscriber: GameMode raw damage handler

StateChanged
  entity: goblin
  field: hp
  before: 12
  after: 6
  source: bleed
```

## Why This Belongs In rpgkit

Some observations are host-specific:

```text
state changed in Unreal fighter map
HUD updated
Blueprint event fired
DataAsset card was clicked
```

But many observations come from the portable core:

```text
topic published
subscriber invoked
modifier added
chain executed
effect applied
effect removed
```

If `rpgkit` exposes those core observations, terminal games, Unreal, Unity, and
tests can all explain combat in the same language.

## Success Criteria

An observability API would satisfy this use case when a host can:

```text
capture a complete trace for one action or round
derive a human combat log from the trace
show chain breakdowns without bespoke host plumbing
debug missing subscribers or wrong target resolution
distinguish gameplay events from observation records
record nested publish flows such as turn.ended -> raw_damage.requested
```
