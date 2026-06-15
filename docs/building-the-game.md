# RPGKit UE — Building the Demo Game

Reference guide for wiring the Unreal RPGKit demo. Keep this open alongside the editor.

---

## Editor cheat sheet

| Where | What it is |
|-------|-----------|
| **Content Browser** (bottom) | File explorer — your assets live in `Content/Blueprints/` |
| **World Outliner** (top-right) | Lists everything in the current level |
| **Details** (bottom-right, right sidebar) | Properties of whatever you selected |
| **Palette** (left, in Widget Designer) | Widgets you drag onto your layout |
| **Hierarchy** (top-center, in Widget Designer) | Tree of all widgets in your layout |
| **Designer tab** | Visual layout of a widget |
| **Graph tab** | Blueprint scripting for a widget or blueprint |

---

## Project files on disk

```
rpgkit-ue/
├── RPGKitUE.uproject                    ← open this to launch editor
├── Content/Blueprints/                  ← your Blueprint assets
│   ├── BP_RPGKitGameMode.uasset         ← GameMode subclass
│   ├── BP_ToughSkin.uasset              ← Tough Skin effect
│   ├── WBP_Card.uasset                  ← individual card widget
│   └── WBP_CombatHUD.uasset             ← full game screen widget
├── Content/Basics.umap                  ← your level
├── Source/RPGKitUE/                     ← C++ code (VS edits, then Ctrl+Alt+F11 in editor)
└── ThirdParty/rpgkit/                   ← rpgkit library headers
```

---

## Part 1 — BP_RPGKitGameMode

### 1.1 Create the HUD on game start

Open `BP_RPGKitGameMode` → **Event Graph** (it opens here by default).

```
[Event BeginPlay]
       │
  ┌────▼─────┐  Create Widget node
  │ Create   │  Class = WBP_CombatHUD
  │ Widget   │
  └────┬─────┘
       │ Return Value (blue pin)
  ┌────▼──────────┐  right-click → Add to Viewport
  │ Add to        │
  │ Viewport      │
  └───────────────┘
```

**How:** Right-click empty graph → type `Create Widget` → set Class to `WBP_CombatHUD`. Drag off the **Return Value** pin → search `Add to Viewport`.

**Compile** and close.

### 1.2 Card definitions (configure in defaults)

With `BP_RPGKitGameMode` closed, **single-click** it in Content Browser (don't double-click — that opens it). In the **Details** panel (bottom-right), find the **RPGKit \| Cards** category:

- **Card Definitions**: Add card data assets such as:

| Asset | Card | Actions |
|-------|------|---------|
| `DA_CardStrike` | Strike, Cost 1 | Damage 5 → Enemy |
| `DA_CardDefend` | Defend, Cost 1 | Block 5 → Self |
| `DA_Card_ShieldBash` | Shield Bash, Cost 2 | Block 4 → Self; Damage 4 → Enemy |

Cards no longer have top-level Damage/Block/Heal fields. Put behavior in the
card's `Actions` array.

- **Current Energy** = 3
- **Max Energy** = 3

---

## Part 2 — WBP_CombatHUD

### 2.1 Variable: GameModeRef

Open `WBP_CombatHUD` → **Graph** tab (top of window, next to Designer).

**My Blueprint** panel (left side of Graph view) → click **+ Variable**:

| Field | Value |
|-------|-------|
| Variable Name | `GameModeRef` |
| Variable Type | Click dropdown → search `RPGKitGameMode` → select it |

**Compile.**

### 2.2 End Turn button click

Still in **Graph** tab of WBP_CombatHUD → **My Blueprint** panel:

1. Find `EndTurnButton` under Variables
2. Select it → look at **Details** panel (bottom-right) → **Events** section
3. Click **+** next to **OnClicked**

Wire:

```
[OnClicked (EndTurnButton)]
       │
  ┌────▼──────────────┐
  │ Get GameModeRef    │  (drag variable from My Blueprint → "Get")
  └────┬───────────────┘
       │
  ┌────▼──────┐
  │ EndTurn   │  (drag off blue pin → type "EndTurn")
  └───────────┘
```

**Compile** and close.

### 2.3 Functions to update the HUD (add in Graph)

In the **My Blueprint** panel → click **+ Function** (under Functions):

**Function 1: UpdateStats**

```
[UpdateStats]
       │
  ┌────▼──────────────┐
  │ Get GameModeRef    │
  └────┬───────────────┘
       │
  ┌────▼──────────────┐
  │ Get Fighter  Id=hero│  (type "Get Fighter")
  └────┬───────────────┘
       │ Return Value → Break FRPGKitFighter
       │
  ┌────▼──────┐    ┌────▼──────┐    ┌────▼──────┐
  │ Set Text  │    │ Set Text  │    │ Set Text  │
  │ HeroName  │    │ HeroHP    │    │ HeroBlock │
  │ = Name    │    │ = Format  │    │ = Block   │
  └───────────┘    └───────────┘    └───────────┘

  (repeat for Id="goblin" → EnemyNameText, EnemyHPText, EnemyBlockText)
```

For HP text: drag off CurrentHP → type `Format Text` → set the format string in Format Text node to `HP: {0}/{1}` with both pins connected.

**Function 2: AddLog**

Add a **Text** input parameter: click the function name → Details → Inputs → + → name it `Message`.

```
[AddLog(Message)]
       │
  ┌────▼────────────┐
  │ Get CombatLogText │  (drag from My Blueprint → "Get CombatLogText")
  └────┬────────────┘
       │
  ┌────▼────────────┐
  │ Set Text         │  (type "Set Text (Text Block)")
  │ Target: CombatLog│
  │ In Text:         │
  │  BuildString...  │  (existing text + "\n" + Message)
  └─────────────────┘
```

### 2.4 Refresh card hand

**Function: RefreshHand**

```
[RefreshHand]
       │
  ┌────▼──────────────┐
  │ Clear Children     │  (type "Clear Children" — on CardHandBox)
  │ Target: CardHandBox│
  └────┬───────────────┘
       │
  ┌────▼──────────────┐
  │ Get GameModeRef    │
  └────┬───────────────┘
       │
  ┌────▼──────────────┐
  │ Get Current Hand   │  (drag off → type "Get Current Hand" — returns array)
  └────┬───────────────┘
       │
  ┌────▼──────────────┐
  │ For Each Loop      │  (drag off array pin)
  └────┬───────────────┘
       │ Array Element → Break FRPGKitCard
       │
  ┌────▼──────────────┐
  │ Create WBP_Card    │  (type "Create Widget" — Class = WBP_Card)
  │ Widget             │
  └────┬───────────────┘
       │ Return Value
  ┌────▼──────────────┐
  │ Add Child          │  (type "Add Child" — Target = CardHandBox)
  │ Target: CardHandBox│
  │ Content: [widget]  │
  └────────────────────┘
```

### 2.5 Energy display

**Function: UpdateEnergy**

Read GameModeRef → Get Current Energy / Get Max Energy → Format Text → Set EnergyText.

---

## Part 3 — WBP_Card

### 3.1 Variables

Open `WBP_Card` → **Graph**:

Add variables (same process as before):

| Name | Type |
|------|------|
| CardIndex | Integer |
| CardData | FRPGKitCard |

### 3.2 Set card data function

**Function: SetCardData** — add two inputs: Index (Integer), Data (FRPGKitCard)

```
[SetCardData(Index, Data)]
       │
  ┌────▼──────┐
  │ Set       │  CardIndex = Index
  │ CardIndex │
  └────┬──────┘
       │
  ┌────▼──────┐
  │ Set       │  CardData = Data
  │ CardData  │
  └────┬──────┘
       │
  ┌────▼────────────┐
  │ Set NameText     │  Text = Data→Name (break the struct)
  │ Set CostText     │  Text = "Cost: X"
  │ Set DescText     │  Text = damage/block/heal description
  └─────────────────┘
```

### 3.3 Play button click

In WBP_Card's Variables → find `PlayButton` → select → Details → Events → + OnClicked.

```
[OnClicked (PlayButton)]
       │
  ┌────▼───────────────────┐
  │ Get Owning Player ...  │  (find GetOwningPlayerPawn and drill up to the HUD somehow...)
  └────────────────────────┘
```

This gets complex. Simpler approach: use an **Event Dispatcher**.

### 3.4 Event Dispatcher for card click

In **My Blueprint** → **+ Event Dispatcher** → name it `OnCardPlayed`.

Add a parameter to it: select OnCardPlayed → Details → **+** Input → name `Index`, type `Integer`.

Now in the PlayButton OnClicked:
```
[OnClicked (PlayButton)]
       │
  ┌────▼──────┐
  │ Get       │  CardIndex
  │ CardIndex │
  └────┬──────┘
       │
  ┌────▼──────────┐
  │ Call          │  OnCardPlayed(Index)
  │ OnCardPlayed  │
  └───────────────┘
```

**Compile** and close.

---

## Part 4 — WBP_CombatHUD ↔ WBP_Card binding

Open `WBP_CombatHUD` → **Graph** → edit the **RefreshHand** function:

After creating the WBP_Card widget and before Add Child, bind the dispatcher:

```
[Create WBP_Card Widget]
       │ Return Value
  ┌────▼──────────────────────┐
  │ Call SetCardData on card  │  (drag off widget → type "SetCardData" Index=loop index, Data=element)
  └────┬──────────────────────┘
       │
  ┌────▼──────────────────────┐
  │ Bind Event to OnCardPlayed│  (drag off widget → type "Bind Event to OnCardPlayed")
  │   Event → Create Event    │  (right-click → "Create Event")
  │   The new event fires     │
  └────┬──────────────────────┘
       │
  ┌────▼──────────────────────┐
  │ GameModeRef → PlayCard    │  call PlayCard with the index from event
  │ GameModeRef → UpdateStats │  refresh stats after playing
  │ this → RefreshHand        │  rebuild card display
  │ this → UpdateEnergy       │  refresh energy
  └──────────────────────────┘
```

---

## Quick reference: common Blueprint nodes

| What to type in search | What it does |
|------------------------|-------------|
| `Create Widget` | Spawns a widget (set Class in Details) |
| `Add to Viewport` | Shows a widget on screen |
| `Add Child` | Puts a widget inside another |
| `Clear Children` | Removes all children from a container |
| `For Each Loop` | Iterates over an array |
| `Set Text (Text Block)` | Changes Text Block text |
| `Format Text` | Builds a string from values `{0}`, `{1}`, etc. |
| `Get Fighter` | Reads a fighter from GameMode |
| `Break FRPGKitFighter` | Split a fighter struct into its fields |
| `PlayCard` / `DealHand` / `EndTurn` | GameMode actions |
| `Bind Event to [Dispatcher]` | Connects a card's click event to a handler |

---

## What's on disk vs what's in memory

| Action | What it does |
|--------|-------------|
| **Compile** (Blueprint) | Saves the Blueprint to disk |
| **Save** (toolbar or Ctrl+S) | Writes the .uasset file |
| **Live Coding (Ctrl+Alt+F11)** | Recompiles C++ while editor runs |
| **Play** | Runs the game in editor |
| **Stop** | Returns to editing |

Always **Compile** then **Save** before closing a Blueprint. Blueprints with unsaved changes show an asterisk (*) on the tab.
