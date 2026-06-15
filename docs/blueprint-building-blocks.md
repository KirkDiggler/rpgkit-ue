# Blueprint Building Blocks

This is the code-first path for setting up cards in the Unreal editor.

The idea is:

```text
C++ defines the shape of a card
Unreal Data Assets hold card data
GameMode draws those assets into CurrentHand
HUD/card widgets call PlayCard(index)
```

## C++ Types Added

Cards now have explicit actions:

```cpp
UENUM(BlueprintType)
enum class ERPGKitCardActionType : uint8
{
    Damage,
    Block,
    Heal,
    ApplyBleed,
    ApplyVulnerable
};
```

```cpp
USTRUCT(BlueprintType)
struct FRPGKitCardAction
{
    ERPGKitCardActionType Type;
    int32 Amount;
    int32 DurationTurns;
    ERPGKitActionTargetMode Target;
    FString ExplicitTargetId;
};
```

Normal cards should use `Target = Self` or `Target = Enemy`. Use
`Target = Explicit` only when the card needs to name a specific runtime id.

`FRPGKitCard` owns an ordered list of actions:

```cpp
TArray<FRPGKitCardAction> Actions;
```

That lets one card do multiple things, for example:

```text
Shield Bash
  Cost: 2
  Actions:
    Block 5 -> Self
    Damage 3 -> Enemy
```

## Card Definition Assets

`URPGKitCardDefinition` is a `UDataAsset` wrapper around one `FRPGKitCard`.

In Unreal terms, this means you can create standalone card assets in the Content
Browser instead of typing every card into the GameMode defaults array.

```cpp
UCLASS(BlueprintType)
class URPGKitCardDefinition : public UDataAsset
{
    FRPGKitCard Card;
};
```

## How GameMode Uses Them

`ARPGKitGameMode` has two deck inputs:

```cpp
TArray<FRPGKitCard> CardPool;
TArray<TObjectPtr<URPGKitCardDefinition>> CardDefinitions;
```

Prefer `CardDefinitions` for authored content. `CardPool` remains useful as a
quick inline/debug array, but it uses the same action-only `FRPGKitCard` shape.

`DealHand` checks `CardDefinitions` first.

If `CardDefinitions` has valid assets, it draws from those.

If not, it falls back to `CardPool`.

So both paths now use the same model: cards declare actions; GameMode interprets
them.

## Minimal Editor Steps

After compiling C++ or using Live Coding:

1. In Content Browser, make a folder like `Content/Cards`.
2. Right-click the folder.
3. Choose `Miscellaneous -> Data Asset`.
4. Pick `RPGKitCardDefinition` as the data asset class.
5. Name it something like `DA_Card_Strike`.
6. Open it and fill out its `Card` field.
7. Open `BP_RPGKitGameMode` defaults.
8. Add the data asset to `RPGKit | Cards -> Card Definitions`.

That is the whole card-definition loop.

## Example Cards

### Strike

```text
Name: Strike
Cost: 1
Actions:
  Type: Damage
  Amount: 5
  Target: Enemy
```

### Defend

```text
Name: Defend
Cost: 1
Actions:
  Type: Block
  Amount: 5
  Target: Self
```

### Heal

```text
Name: Heal
Cost: 2
Actions:
  Type: Heal
  Amount: 4
  Target: Self
```

### Shield Bash

```text
Name: Shield Bash
Cost: 2
Actions:
  Type: Block
  Amount: 4
  Target: Self

  Type: Damage
  Amount: 4
  Target: Enemy
```

### Expose Weakness

```text
Name: Expose Weakness
Cost: 1
Actions:
  Type: Apply Vulnerable
  Amount: 50
  Duration Turns: 2
  Target: Enemy
```

For `Apply Vulnerable`, `Amount` means percent bonus damage. `50` means the
target takes +50% damage while the effect is active.

### Rend

```text
Name: Rend
Cost: 1
Actions:
  Type: Apply Bleed
  Amount: 3
  Duration Turns: 2
  Target: Enemy
```

For this first workshop pass, `Apply Bleed` uses `Amount` as stack count and
`Duration Turns` as damage per stack. This is intentionally temporary pressure on
the action-spec model; we will likely replace it with effect-specific specs.

## Target Modes

Use target modes for common cases:

| Target mode | Resolves to |
|---|---|
| Self | the current actor, currently `hero` |
| Enemy | the current enemy, currently `goblin` |
| Explicit | `ExplicitTargetId`, falling back to `goblin` if empty |

The hardcoded actor/enemy ids are still demo scaffolding inside `PlayCard`, but
card assets no longer need to know those ids for normal actions.

## What Happens When A Card Is Played

The HUD/card widget should eventually call:

```cpp
ARPGKitGameMode::PlayCard(CardIndex)
```

`PlayCard` does this:

```text
validate index
check energy
subtract cost
if card has no actions, log that fact
for each action:
  Damage -> Strike("hero", target, amount)
  Block  -> AddBlock(target, amount)
  Heal   -> mutate fighter HP and log
  Apply Bleed -> create/apply a Bleed effect on target
  Apply Vulnerable -> create/apply a Vulnerable effect on target
remove card from CurrentHand
OnHandChanged()
```

Current limitation: multiple Vulnerable effects on the same target are not yet
stack-safe. The first workshop version is for proving the effect/action/bus path.

So the widget does not need to know card rules. It only needs to know which card
index was clicked.

## Current Next Step

The next useful Blueprint task is not the whole HUD.

The next useful task is only:

```text
show CurrentHand
for each card, make a card widget
when clicked, call PlayCard(index)
```

Everything else can come after that.
