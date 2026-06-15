# RPGKit UE - Code-First Tour

This doc is for reading the Unreal demo as code first, Blueprint second.

The goal is not to memorize Unreal editor clicks. The goal is to understand how
the pure C++ `rpgkit` concepts show up in an Unreal project, then use Blueprints
as a thin visual layer over those C++ seams.

## The Big Shape

```text
ThirdParty/rpgkit/core/include/rpg/core/*
        pure C++20 library: Bus, Topic, Chain, Effect

Source/RPGKitUE/*
        Unreal adapter layer: USTRUCT, UCLASS, UFUNCTION, UPROPERTY

Content/Blueprints/*
        visual assets: GameMode subclass, HUD widget, card widget
```

The important source files are:

| File | Role |
|---|---|
| `RPGKitGameMode.h/.cpp` | Demo game state and combat orchestration |
| `RPGKitBus.h/.cpp` | Unreal subsystem that owns the `rpg::core::Bus` |
| `RPGKitEffect.h/.cpp` | Unreal `UObject` wrappers around persistent `rpg::core::Effect`s |
| `rpg/core/bus.hpp` | Type-erased synchronous event router |
| `rpg/core/topic.hpp` | Typed wrappers over the erased bus |
| `rpg/core/chain.hpp` | Per-resolution modifier collector/executor |
| `rpg/core/effect.hpp` | Persistent listener lifecycle: apply, track subscriptions, remove |

If you know the Go terminal demo, map this Unreal project as:

```text
terminal main loop       -> ARPGKitGameMode methods
Go structs               -> USTRUCT(BlueprintType)
Go exported methods      -> UFUNCTION(BlueprintCallable)
log/output callbacks     -> BlueprintImplementableEvent
global/session bus       -> UGameInstanceSubsystem owning rpg::core::Bus
```

## Unreal Reflection In Plain Terms

Unreal needs metadata to see C++ types in the editor and in Blueprints.

`USTRUCT(BlueprintType)` means: this C++ struct can be used as a Blueprint data
type.

Example: `FRPGKitFighter` in `RPGKitGameMode.h`.

```cpp
USTRUCT(BlueprintType)
struct FRPGKitFighter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
    FString Name;
};
```

`UPROPERTY(...)` means: Unreal can reflect, serialize, edit, or expose this
field depending on the flags.

`UCLASS()` means: this C++ class participates in Unreal object/class metadata.

`UFUNCTION(BlueprintCallable)` means: Blueprint can call this C++ function.

`UFUNCTION(BlueprintImplementableEvent)` means: C++ declares the event, but a
Blueprint subclass supplies the body.

That last one is the C++ to UI edge. For example, C++ calls:

```cpp
OnCombatLog(TEXT("hero strikes goblin"));
```

The Blueprint decides whether that message becomes a print string, a HUD log
line, a sound, or nothing.

## GameMode: The Demo's Orchestrator

`ARPGKitGameMode` inherits from Unreal's `AGameModeBase`.

Conceptually, it is the demo game's coordinator:

```cpp
class ARPGKitGameMode : public AGameModeBase
{
    TMap<FString, FRPGKitFighter> Fighters;
    TArray<FRPGKitCard> CurrentHand;
    TObjectPtr<URPGKitBus> BusSubsystem;
    TArray<URPGKitEffect*> ActiveEffects;
};
```

The Unreal base class gives it a lifetime and a role in the current level. The
RPGKit-specific behavior is in this project's C++ methods.

Important public methods:

| Method | Direction | Meaning |
|---|---|---|
| `SetupEncounter` | Blueprint/UI -> C++ | Put hero/enemy into the fight map |
| `DealHand` | Blueprint/UI -> C++ | Fill `CurrentHand` from card definitions or `CardPool` |
| `PlayCard` | Blueprint/UI -> C++ | Spend energy and apply card effects |
| `Strike` | Blueprint/UI or C++ -> C++ | Resolve damage through the RPGKit chain |
| `ApplyEffect` | Blueprint/UI -> C++ | Register a persistent effect with the bus |
| `EndTurn` | Blueprint/UI -> C++ | Publish `turn.ended` and reset block |
| `OnCombatLog` | C++ -> Blueprint/UI | Tell UI that text happened |
| `OnHandChanged` | C++ -> Blueprint/UI | Tell UI to redraw the hand |

The key thing: Blueprint is not the game engine here. `ARPGKitGameMode` is.
Blueprints mostly push buttons and render state.

## Bus Ownership

The pure library has `rpg::core::Bus`.

Unreal wraps it in `URPGKitBus`, a `UGameInstanceSubsystem`:

```cpp
UCLASS()
class URPGKitBus : public UGameInstanceSubsystem
{
    GENERATED_BODY()

private:
    TUniquePtr<rpg::core::Bus> RawBus;
};
```

Why a subsystem?

Because Unreal controls object lifetimes. A `UGameInstanceSubsystem` gives us one
bus for the running game session without making random actors own it.

The bus is created here:

```cpp
void URPGKitBus::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RawBus = MakeUnique<rpg::core::Bus>();
}
```

`ARPGKitGameMode::SetupEncounter` caches a pointer to that subsystem:

```cpp
if (UGameInstance* GI = GetGameInstance())
{
    BusSubsystem = GI->GetSubsystem<URPGKitBus>();
}
```

This is the Unreal equivalent of saying: "give this encounter code access to the
session's event bus."

## Topics Defined Once

The Unreal bridge defines topic ids in `RPGKitBus.h`:

```cpp
namespace RPGKitTopics
{
    inline rpg::core::TopicDef<int32> kTurnEnded("turn.ended");
    inline rpg::core::TopicDef<FRPGKitDamageEvent> kCombatDamage("combat.damage");
    inline std::vector<std::string> kDamageStages = {"base", "effects", "final"};
}
```

This matches the core library pattern:

```cpp
TopicDef<T> definition("topic.id");
Topic<T> notificationTopic = definition.on(bus);
ChainedTopic<T> chainedTopic = definition.onChained(bus);
```

The topic definition is static rulebook data. Binding it with `.on(bus)` or
`.onChained(bus)` creates a runtime topic attached to one concrete bus.

## Two Topic Flavors

The pure `rpgkit` topic layer has two different concepts.

### Notification Topic

`Topic<T>` means: publish a value, handlers observe it.

Example in this project: `turn.ended`.

```cpp
rpg::core::Topic<int32> topic =
    RPGKitTopics::kTurnEnded.on(BusSubsystem->GetRawBus());

(void)topic.publish(TurnNumber);
```

Handlers receive `const int32&` and do whatever they do.

### Chained Topic

`ChainedTopic<T>` means: publish a value plus a mutable `Chain<T>`. Handlers do
not transform the value directly. They add modifiers to the chain.

Example in this project: `combat.damage`.

```cpp
rpg::core::ChainedTopic<FRPGKitDamageEvent> topic =
    RPGKitTopics::kCombatDamage.onChained(*RawBus);

rpg::core::Status status = topic.publish(Event, chain);
```

The important rule:

```text
publish collects modifiers
execute applies modifiers
```

That separation is the heart of the design.

## Event Walkthrough: Tough Skin Modifies Damage

This is the best first event path to study because it crosses every boundary:

```text
Blueprint/user applies Tough Skin
        -> ARPGKitGameMode::ApplyEffect
        -> URPGKitBus::ApplyEffect
        -> rpg::core::Effect::apply
        -> FToughSkinEffect::onApply subscribes to combat.damage

Later, user/card calls Strike
        -> ARPGKitGameMode::Strike
        -> URPGKitBus::ExecuteDamageChain
        -> publish combat.damage
        -> Tough Skin subscriber adds a damage reducer to the chain
        -> chain.execute applies reducer
        -> GameMode subtracts final damage from goblin HP
        -> GameMode fires Blueprint UI events
```

### Step 1: The Unreal Object Exists

The Blueprint asset `BP_ToughSkin` is backed by this C++ type:

```cpp
UCLASS(BlueprintType)
class URPGKitToughSkinEffect : public URPGKitEffect
```

`URPGKitEffect` is a `UObject` wrapper around the pure library's
`rpg::core::Effect`:

```cpp
class URPGKitEffect : public UObject
{
protected:
    rpg::core::Effect* RawEffectPtr = nullptr;
};
```

The Unreal object exists so Blueprint can create/configure it. The real RPGKit
behavior lives in an inner C++ effect implementation.

### Step 2: The Constructor Creates The Inner Effect

`URPGKitToughSkinEffect::URPGKitToughSkinEffect` does this:

```cpp
InnerEffectImpl = new FToughSkinEffect(*this);
RawEffectPtr = InnerEffectImpl;
```

`FToughSkinEffect` inherits from the pure core type:

```cpp
class URPGKitToughSkinEffect::FToughSkinEffect : public rpg::core::Effect
```

This is the adapter pattern in miniature:

```text
UObject visible to Unreal
        owns / points at
pure C++ rpg::core::Effect visible to rpgkit
```

### Step 3: GameMode Applies The Effect

`ARPGKitGameMode::ApplyEffect` is Blueprint-callable:

```cpp
bool ARPGKitGameMode::ApplyEffect(URPGKitEffect* Effect)
{
    if (!BusSubsystem || !Effect) return false;

    bool bSuccess = BusSubsystem->ApplyEffect(Effect);
    if (bSuccess)
    {
        ActiveEffects.Add(Effect);
        Effect->OnEffectApplied();
        OnEffectApplied(Effect->GetName());
    }
    return bSuccess;
}
```

Read this as:

```text
validate inputs
ask bus subsystem to apply the raw effect
if it worked, keep the UObject alive in ActiveEffects
notify Blueprint/UI
```

The `ActiveEffects` array matters because Unreal garbage collection owns
`UObject` lifetimes. Keeping a `UPROPERTY()` reference prevents the effect object
from disappearing while its raw C++ effect is subscribed to the bus.

### Step 4: Bus Subsystem Calls Pure rpgkit

`URPGKitBus::ApplyEffect` is tiny:

```cpp
bool URPGKitBus::ApplyEffect(URPGKitEffect* Effect)
{
    if (!RawBus || !Effect) return false;

    rpg::core::Status status = Effect->GetRawEffect().apply(*RawBus);
    if (!status.isOk())
    {
        UE_LOG(LogTemp, Warning, TEXT("RPGKit: apply effect failed: %s"),
            UTF8_TO_TCHAR(status.message().c_str()));
        return false;
    }
    return true;
}
```

This is the boundary between Unreal and pure rpgkit:

```text
URPGKitEffect*          Unreal object
Effect->GetRawEffect() pure rpg::core::Effect&
RawBus                 pure rpg::core::Bus
```

### Step 5: Core Effect Applies Itself

In `rpg/core/effect.hpp`, `Effect::apply` does the lifecycle work:

```cpp
Status apply(Bus& bus) {
    if (active_) {
        return Status::error("effect already active: " + id_);
    }
    bus_ = &bus;
    tracked_.clear();
    Status applied = onApply(bus);
    if (!applied.isOk()) {
        for (const SubscriptionId sub : tracked_) {
            (void)bus.unsubscribe(sub);
        }
        tracked_.clear();
        bus_ = nullptr;
        return applied;
    }
    active_ = true;
    return Status::ok();
}
```

The important design choices:

| Choice | Why it matters |
|---|---|
| Effects are persistent listeners | They stay subscribed between attacks/turns |
| `onApply(bus)` is virtual | Each effect defines its own subscriptions |
| Subscriptions are tracked | `remove()` can unsubscribe automatically |
| The bus pointer is remembered | Removing from the wrong bus is not possible |

### Step 6: Tough Skin Subscribes To Damage

`FToughSkinEffect::onApply` binds the static topic definition to this runtime bus:

```cpp
rpg::core::ChainedTopic<FRPGKitDamageEvent> topic =
    RPGKitTopics::kCombatDamage.onChained(bus);
```

Then it subscribes a handler:

```cpp
auto id = topic.subscribe([this](const FRPGKitDamageEvent& event,
                                 rpg::core::Chain<FRPGKitDamageEvent>& chain) -> rpg::core::Status {
    if (event.TargetId != TCHAR_TO_UTF8(*Owner.ProtectedEntityId))
    {
        return rpg::core::Status::ok();
    }

    const int32 reduction = Owner.DamageReduction;
    return chain.add("effects",
        TCHAR_TO_UTF8(*FString::Printf(TEXT("tough-skin-%s"), *Owner.ProtectedEntityId)),
        [reduction](FRPGKitDamageEvent e) -> FRPGKitDamageEvent {
            e.BaseAmount = FMath::Max(0, e.BaseAmount - reduction);
            return e;
        });
});
```

Then it tracks the subscription id:

```cpp
track(id);
```

That one line is what lets `rpg::core::Effect::remove()` clean up later.

The handler does not mutate the incoming event. It says:

```text
if this damage is against my protected entity,
add a modifier function to the effects stage of the chain
```

That modifier function is this lambda:

```cpp
[reduction](FRPGKitDamageEvent e) -> FRPGKitDamageEvent {
    e.BaseAmount = FMath::Max(0, e.BaseAmount - reduction);
    return e;
}
```

This is close to a Go function value:

```go
func(e DamageEvent) DamageEvent {
    e.BaseAmount = max(0, e.BaseAmount-reduction)
    return e
}
```

## Event Walkthrough: Strike Publishes Damage

Now assume Tough Skin is active. The next interesting path starts with
`ARPGKitGameMode::Strike`.

### Step 1: GameMode Builds A Damage Event

```cpp
FRPGKitDamageEvent Event;
Event.AttackerId = AttackerId;
Event.TargetId = TargetId;
Event.BaseAmount = BaseDamage;
```

This struct is Unreal-reflected, but it is also the payload type for the pure
`rpgkit` chained topic.

### Step 2: GameMode Asks The Bus To Resolve Damage

```cpp
FRPGKitChainResult Result = BusSubsystem->ExecuteDamageChain(Event);
```

This is where the pure RPGKit chain runs.

### Step 3: Bus Creates A Fresh Chain

`URPGKitBus::ExecuteDamageChain` starts with:

```cpp
rpg::core::Chain<FRPGKitDamageEvent> chain(RPGKitTopics::kDamageStages);
```

Important: the chain is per-resolution. It is born for this one strike and then
discarded.

This is what prevents stale modifiers from accumulating accidentally.

### Step 4: Publish Collects Contributions

```cpp
rpg::core::ChainedTopic<FRPGKitDamageEvent> topic =
    RPGKitTopics::kCombatDamage.onChained(*RawBus);

rpg::core::Status status = topic.publish(Event, chain);
```

Under the hood, `ChainedTopic<T>::publish` packages pointers to the event and
chain into an envelope and sends it through the erased bus:

```cpp
return bus_->publish(id_, std::any(Envelope{.event = &event, .chain = &chain}));
```

The bus synchronously calls every subscriber on `combat.damage` in subscription
order.

If Tough Skin is subscribed, its handler gets called and adds a modifier to the
chain.

At this moment, damage has still not changed.

### Step 5: Execute Applies Contributions

After publish returns:

```cpp
auto chainResult = chain.execute(Event);
Result.Value = chainResult.value.BaseAmount;
```

`Chain<T>::execute` folds the event through stages:

```cpp
for (const std::string& stage : stages_) {
    for (const Entry& entry : entries_) {
        if (entry.stage != stage) continue;
        T before = result.value;
        result.value = entry.modifier(std::move(result.value));
        result.breakdown.push_back(...);
    }
}
```

With Tough Skin active, a strike for 5 damage against goblin becomes:

```text
input event: BaseAmount = 5
effects/tough-skin-goblin: 5 -> 4
final result: BaseAmount = 4
```

The returned breakdown is not just debugging. It is first-class output that the
UI can show as a combat receipt.

### Step 6: Convert Back To Unreal Types

The bridge converts the pure chain result into `FRPGKitChainResult`:

```cpp
for (const auto& step : chainResult.breakdown)
{
    FRPGKitChainStep S;
    S.Stage = UTF8_TO_TCHAR(step.stage.c_str());
    S.ModifierId = UTF8_TO_TCHAR(step.id.c_str());
    S.Before = step.before.BaseAmount;
    S.After = step.after.BaseAmount;
    Result.Breakdown.Add(S);
}
```

This is mostly string/type conversion:

```text
std::string      -> FString
std::vector      -> TArray
chain Step<T>    -> FRPGKitChainStep
```

### Step 7: GameMode Applies Final Damage To State

Back in `ARPGKitGameMode::Strike`:

```cpp
int32 FinalDamage = Result.Value;

if (FRPGKitFighter* Target = FindFighter(TargetId))
{
    int32 Blocked = FMath::Min(Target->Block, FinalDamage);
    Target->Block -= Blocked;
    int32 HPDamage = FinalDamage - Blocked;
    Target->CurrentHP = FMath::Max(0, Target->CurrentHP - HPDamage);
}
```

Notice the boundary:

```text
rpgkit chain decides final damage number
GameMode mutates demo game state
```

The pure library does not know about this demo's fighter map. That is an
application concern.

### Step 8: GameMode Emits UI Events

After mutating state, GameMode tells Blueprint/UI what happened:

```cpp
OnDamageDealt(Result);
OnCombatLog(...);
OnFighterDied(TargetId);
```

This is the second major edge:

```text
C++ gameplay -> Blueprint presentation
```

If the Blueprint has implemented those events, the HUD updates. If it has not,
the game logic still ran.

## Event Walkthrough: End Turn And Bleed

The turn event is simpler because it is a notification topic, not a chained
topic.

`ARPGKitGameMode::EndTurn` does this:

```cpp
TurnNumber++;

rpg::core::Topic<int32> topic =
    RPGKitTopics::kTurnEnded.on(BusSubsystem->GetRawBus());

(void)topic.publish(TurnNumber);
```

`URPGKitBleedEffect::FBleedEffect::onApply` subscribes to that topic:

```cpp
rpg::core::Topic<int32> topic =
    RPGKitTopics::kTurnEnded.on(bus);

auto id = topic.subscribe([this](const int32& turnNumber) -> rpg::core::Status {
    // tick bleed here
    return rpg::core::Status::ok();
});

track(id);
```

Current shape: Bleed subscribes to `turn.ended`, publishes a typed
`combat.raw_damage.requested` intent, and `ARPGKitGameMode` handles that request
by mutating fighter HP with `DealRawDamage`. That keeps effect ticking on the bus
while keeping game-state mutation inside the encounter runtime.

So Tough Skin is the better path to study first for chained damage modifiers,
while Bleed is the better path for nested synchronous event flow:
`turn.ended -> raw_damage.requested -> DealRawDamage`.

## The Blueprint Layer In This Mental Model

Blueprints should feel less mysterious if you view them as reflection clients.

When Blueprint calls `Strike`, it is calling this exact C++ method:

```cpp
UFUNCTION(BlueprintCallable, Category = "RPGKit")
FRPGKitChainResult Strike(const FString& AttackerId,
                          const FString& TargetId,
                          int32 BaseDamage);
```

When Blueprint handles `OnCombatLog`, it is implementing this C++ declaration:

```cpp
UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
void OnCombatLog(const FString& Message);
```

So the Blueprint graph is not a separate architecture. It is just a visual call
site or visual event body for reflected C++.

## How To Debug The Edges

When something does not happen, ask which edge failed.

### Did Blueprint Call C++?

Check whether the function has `UFUNCTION(BlueprintCallable)` and whether the
Blueprint is calling the right object instance.

Common example: `Get Game Mode` returns `AGameModeBase*`, so Blueprint must cast
to `RPGKitGameMode` to call RPGKit-specific methods.

### Did C++ Find The Bus?

`SetupEncounter` currently caches `BusSubsystem`. If methods run before setup,
`BusSubsystem` may be null.

### Was The Effect Applied?

`ApplyEffect` should add the effect to `ActiveEffects` and call
`OnEffectApplied`.

### Was The Effect Subscribed To The Right Topic Flavor?

Damage effects use:

```cpp
kCombatDamage.onChained(bus)
```

Turn listeners use:

```cpp
kTurnEnded.on(bus)
```

Mixing topic flavors on one id creates a payload mismatch error.

### Did Publish Happen?

For damage, publish happens in `URPGKitBus::ExecuteDamageChain`.

For turns, publish happens in `ARPGKitGameMode::EndTurn`.

### Did The Chain Execute?

For chained topics, publish alone does not modify data. Look for:

```cpp
chain.execute(Event)
```

### Did C++ Notify Blueprint?

Look for calls like:

```cpp
OnCombatLog(...);
OnHandChanged();
OnDamageDealt(Result);
```

If the C++ call happens but the UI does not change, the missing piece is likely
the Blueprint implementation of that event.

## A Useful Reading Order

Read these in order:

1. `ThirdParty/rpgkit/core/include/rpg/core/bus.hpp`
2. `ThirdParty/rpgkit/core/include/rpg/core/topic.hpp`
3. `ThirdParty/rpgkit/core/include/rpg/core/chain.hpp`
4. `ThirdParty/rpgkit/core/include/rpg/core/effect.hpp`
5. `Source/RPGKitUE/RPGKitBus.h`
6. `Source/RPGKitUE/RPGKitBus.cpp`
7. `Source/RPGKitUE/RPGKitEffect.h`
8. `Source/RPGKitUE/RPGKitEffect.cpp`
9. `Source/RPGKitUE/RPGKitGameMode.h`
10. `Source/RPGKitUE/RPGKitGameMode.cpp`

Then open the Blueprint assets and ask only one question:

```text
Which reflected C++ function/event is this visual node connected to?
```

That keeps Blueprints grounded in code instead of feeling like a second language
you have to learn all at once.

## Current Demo Gaps Worth Knowing

These are not conceptual problems, just current implementation state:

1. The HUD/card assets were recovered from autosaves and have internal names
   like `WPB_CombatHud` and `WPB_Card`. Rename them inside Unreal if you want
   canonical `WBP_*` names.
2. `DefaultEngine.ini` points at `/Game/Maps/L_Main`, but the only map found on
   disk is currently `/Game/Basics`.
3. Bleed is connected through `combat.raw_damage.requested`; the temporary rough
   edge is that `Apply Bleed` reuses `DurationTurns` as damage per stack.
4. The GameMode Blueprint currently appears mostly data-only from the asset
   strings, so some UI event wiring may still need to be recreated in the
   editor.
