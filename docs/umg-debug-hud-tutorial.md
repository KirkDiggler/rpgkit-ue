# UMG Debug Combat HUD Tutorial

This tutorial builds the first real in-game UI for the RPGKit Unreal workshop.

The goal is not a pretty UI. The goal is to replace keyboard/debug-log-only play
with a visible HUD that reads GameMode state and sends player intent back to C++.

## Mental Model

```text
ARPGKitGameMode owns gameplay state and rules.
WBP_DebugCombatHUD displays that state.
Buttons call PlayCard(index) or EndTurn().
GameMode events tell the HUD to refresh.
```

The HUD does not know combat rules.

It only knows:

```text
show current state
show current hand
call GameMode when clicked
```

## What We Are Building

Create a new Widget Blueprint:

```text
WBP_DebugCombatHUD
```

It will show:

```text
Stanthony HP / Block / Energy / Turn
Dirty Gobbo HP / Block

[0 Card] [1 Card] [2 Card] [3 Card] [4 Card]
[End Turn]

Combat Log
Latest Damage Chain
```

For the first version, use five fixed buttons for hand slots 0-4. Later we can
replace this with dynamic card widgets.

## C++ Functions The HUD Uses

From `ARPGKitGameMode`:

```cpp
GetFighter("hero")
GetFighter("goblin")
GetTurnNumber()
GetHandCardSummary(index)
GetRecentCombatLog()
GetLatestDamageBreakdown()
GetDamageBreakdownSummary(index)
PlayCard(index)
EndTurn()
```

The HUD is just a visual caller/reader of these functions.

## Step 1: Create The Widget

In Content Browser:

```text
Content/Blueprints
  Right click -> User Interface -> Widget Blueprint
  Name: WBP_DebugCombatHUD
```

Open it.

## Step 2: Add Layout Widgets

In the Designer tab, create this hierarchy:

```text
Canvas Panel
  Vertical Box RootBox
    TextBlock HeroText
    TextBlock EnemyText
    TextBlock EnergyTurnText
    Horizontal Box HandBox
      Button CardButton0
        TextBlock CardText0
      Button CardButton1
        TextBlock CardText1
      Button CardButton2
        TextBlock CardText2
      Button CardButton3
        TextBlock CardText3
      Button CardButton4
        TextBlock CardText4
    Button EndTurnButton
      TextBlock EndTurnText
    TextBlock CombatLogText
    TextBlock ChainBreakdownText
```

Set `EndTurnText` to:

```text
End Turn
```

For every widget you need in Blueprint Graph, check:

```text
Is Variable
```

Important widgets:

```text
HeroText
EnemyText
EnergyTurnText
CardButton0..4
CardText0..4
EndTurnButton
CombatLogText
ChainBreakdownText
```

## Step 3: Add GameModeRef Variable

Open the Graph tab.

Create variable:

```text
Name: GameModeRef
Type: RPGKitGameMode Object Reference
```

This is the HUD's pointer to the C++ GameMode.

## Step 4: Initialize The HUD

In `WBP_DebugCombatHUD` Graph:

```text
Event Construct
  -> Get Game Mode
  -> Cast To RPGKitGameMode
  -> Set GameModeRef
  -> RefreshHUD
```

This is the same cast pattern as the Level Blueprint keys, but stored once.

## Step 5: Create RefreshHUD Function

Create a function:

```text
RefreshHUD
```

It should read state from `GameModeRef` and update text widgets.

### Fighter Text

For hero:

```text
GameModeRef -> Get Fighter(Id = "hero")
  -> Break RPGKitFighter
  -> Format Text: "Stanthony HP: {CurrentHP}/{MaxHP} Block: {Block}"
  -> SetText(HeroText)
```

For enemy:

```text
GameModeRef -> Get Fighter(Id = "goblin")
  -> Break RPGKitFighter
  -> Format Text: "Dirty Gobbo HP: {CurrentHP}/{MaxHP} Block: {Block}"
  -> SetText(EnemyText)
```

For energy/turn:

```text
GameModeRef CurrentEnergy
GameModeRef MaxEnergy
GameModeRef GetTurnNumber
  -> Format Text: "Energy: {Current}/{Max} Turn: {Turn}"
  -> SetText(EnergyTurnText)
```

### Card Button Text

For each fixed card slot:

```text
GameModeRef -> GetHandCardSummary(0) -> SetText(CardText0)
GameModeRef -> GetHandCardSummary(1) -> SetText(CardText1)
GameModeRef -> GetHandCardSummary(2) -> SetText(CardText2)
GameModeRef -> GetHandCardSummary(3) -> SetText(CardText3)
GameModeRef -> GetHandCardSummary(4) -> SetText(CardText4)
```

This is not dynamic yet, but it is easy to read and debug.

### Combat Log Text

First version can be simple:

```text
GameModeRef -> GetRecentCombatLog
  -> join lines with newline
  -> SetText(CombatLogText)
```

Blueprint does not have the nicest built-in array string join, so if this is
annoying, skip it at first. The C++ debug HUD already shows the recent log.

### Chain Breakdown Text

Same idea:

```text
GameModeRef -> GetLatestDamageBreakdown
  -> ForEach index
  -> GetDamageBreakdownSummary(index)
  -> append lines
  -> SetText(ChainBreakdownText)
```

If this feels too fiddly, skip it for first pass. The C++ debug HUD already shows
the chain breakdown.

## Step 6: Wire Card Buttons

For each button, add `OnClicked` event.

```text
CardButton0 OnClicked
  -> GameModeRef -> PlayCard(0)
  -> RefreshHUD

CardButton1 OnClicked
  -> GameModeRef -> PlayCard(1)
  -> RefreshHUD

CardButton2 OnClicked
  -> GameModeRef -> PlayCard(2)
  -> RefreshHUD

CardButton3 OnClicked
  -> GameModeRef -> PlayCard(3)
  -> RefreshHUD

CardButton4 OnClicked
  -> GameModeRef -> PlayCard(4)
  -> RefreshHUD
```

If a card is removed from the hand, `RefreshHUD` updates the button text.

## Step 7: Wire End Turn Button

```text
EndTurnButton OnClicked
  -> GameModeRef -> EndTurn
  -> RefreshHUD
```

EndTurn currently:

```text
publishes turn.ended
enemy attacks
clears block
deals a new hand
```

So the refresh after EndTurn should show the new hand and updated HP/block.

## Step 8: Add HUD To Viewport

Open `BP_RPGKitGameMode` Event Graph.

After `DealHand`, add:

```text
Create Widget
  Class: WBP_DebugCombatHUD
  -> Add To Viewport
```

Current expected shape:

```text
BeginPlay
  -> SetupEncounter
  -> DealHand
  -> CreateWidget(WBP_DebugCombatHUD)
  -> AddToViewport
```

Compile and save.

## Step 9: Test

Press Play.

Expected:

```text
HUD appears
five card buttons show hand summaries
clicking a card plays it
End Turn triggers enemy attack and new hand
HUD refreshes
```

If nothing appears:

```text
confirm BP_RPGKitGameMode is the active GameMode
confirm CreateWidget class is WBP_DebugCombatHUD
confirm AddToViewport is connected
```

If card buttons do nothing:

```text
confirm GameModeRef is set in Event Construct
confirm button OnClicked calls PlayCard on GameModeRef
confirm CardIndex matches the button slot
```

## Why This Is The Right First HUD

It proves the UI architecture:

```text
GameMode state -> Widget display
Widget click -> GameMode intent
GameMode rules -> state changes
Widget refresh -> visible result
```

Later improvements:

```text
dynamic card widgets instead of five fixed buttons
icons/status badges
proper combat log scroll box
chain breakdown panel
target selection UI
enemy intent preview
```
