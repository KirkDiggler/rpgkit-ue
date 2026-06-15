# Observability Design Sketch

This is a first-pass design sketch, not a final API.

The purpose is to prepare a strong issue/design proposal for `rpgkit` core.

## Distinction: Gameplay Events vs Observable Events

Gameplay events drive rules:

```text
combat.damage
turn.ended
combat.raw_damage.requested
```

Observable events explain engine activity:

```text
TopicPublished
SubscriberInvoked
ModifierAdded
ChainExecuted
EffectApplied
```

Observable events should not drive game rules. They are for logging, debugging,
visualization, testing, and teaching.

## Candidate Observation Types

Core-level observations:

```text
BusSubscribed
BusUnsubscribed
TopicPublished
SubscriberInvoked
SubscriberFailed

ChainedTopicPublished
ModifierAdded
ModifierRejected
ChainExecuted
ChainStepResolved

EffectApplyStarted
EffectApplied
EffectApplyFailed
EffectRemoved
```

Host-level observations:

```text
ActionStarted
ActionResolved
TargetResolved
ResourceSpent
StateChanged
CombatLogLineProduced
HudUpdated
```

Adapter-level observations for `rpgkit-ue`:

```text
BlueprintEventFired
DataAssetResolved
CardActionExecuted
RawDamageRequestHandled
```

## Design Requirements

The API should be:

```text
synchronous with the core bus
low overhead when unused
structured enough for tooling
optional for hosts
safe for nested publish flows
not coupled to Unreal or terminal UI
not a gameplay event bus replacement
```

## Possible API Shapes

### Observer Interface

```cpp
class Observer {
 public:
  virtual void onObservation(const Observation& observation) = 0;
};
```

Core objects could accept an optional observer pointer or sink.

### Function Callback

```cpp
using ObservationSink = std::function<void(const Observation&)>;
```

This is flexible but requires care around allocations and lifetime.

### Trace Object

```cpp
Trace trace;
bus.setTrace(&trace);
```

The trace records observations for later inspection. This may be useful in tests
and tutorials.

## Open Design Questions

1. Should observation payloads be strongly typed structs or a tagged union?
2. Does core own observation type definitions, or should each module define them?
3. Should observations include stable correlation ids for nested actions/chains?
4. How does a host correlate `ActionStarted` with later `TopicPublished` calls?
5. Should chain breakdown remain separate, or also emit `ChainStepResolved` observations?
6. How do we preserve no-exceptions/no-throw behavior in observers?
7. Should observers be allowed to fail, or must they be fire-and-forget?
8. What is the minimum useful v1?

## Minimum Useful v1

For the Unreal demo, the minimum useful core observations are:

```text
TopicPublished
SubscriberInvoked
ModifierAdded
ChainExecuted
EffectApplied
EffectRemoved
```

Host-side `rpgkit-ue` can add:

```text
CardActionExecuted
TargetResolved
StateChanged
```

This would let the demo replace most manual combat logging with structured trace
formatting.
