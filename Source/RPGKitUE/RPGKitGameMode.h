// Demo GameMode that orchestrates rpgkit combat.
// Ports the tutorial examples (02-08) into Unreal's actor/gamemode framework.
// Blueprints override the "On..." events for UI; call the "Do..." functions for gameplay.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/GameModeBase.h"
#include "RPGKitBus.h"
#include "RPGKitGameMode.generated.h"

class URPGKitBus;
class URPGKitEffect;
struct FRPGKitActionExecutor;

// =========================================================================
//  Fighter state (mirrors tutorials 02-05)
// =========================================================================

USTRUCT(BlueprintType)
struct FRPGKitFighter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 MaxHP = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 CurrentHP = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 Block = 0;

	bool IsAlive() const { return CurrentHP > 0; }
};

// =========================================================================
//  Card data (mirrors tutorials 03-05)
// =========================================================================

UENUM(BlueprintType)
enum class ERPGKitCardActionType : uint8
{
	Damage UMETA(DisplayName = "Damage"),
	Block UMETA(DisplayName = "Block"),
	Heal UMETA(DisplayName = "Heal"),
	ApplyBleed UMETA(DisplayName = "Apply Bleed"),
	ApplyVulnerable UMETA(DisplayName = "Apply Vulnerable")
};

UENUM(BlueprintType)
enum class ERPGKitActionTargetMode : uint8
{
	Self UMETA(DisplayName = "Self"),
	Enemy UMETA(DisplayName = "Enemy"),
	Explicit UMETA(DisplayName = "Explicit")
};

USTRUCT(BlueprintType)
struct FRPGKitCardAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	ERPGKitCardActionType Type = ERPGKitCardActionType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 Amount = 0;

	// Used by duration-based actions such as Apply Vulnerable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 DurationTurns = 0;

	// Most cards should use Self or Enemy. ExplicitTargetId is for unusual cases.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	ERPGKitActionTargetMode Target = ERPGKitActionTargetMode::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString ExplicitTargetId;
};

struct FRPGKitActionContext
{
	FString ActorId;
	FString EnemyId;
	FString SourceName;
};

USTRUCT(BlueprintType)
struct FRPGKitCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 Cost = 0;

	// One card can execute multiple explicit actions in order.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	TArray<FRPGKitCardAction> Actions;

	bool IsPlayable(int32 Energy) const { return Cost <= Energy; }
};

UCLASS(BlueprintType)
class URPGKitCardDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RPGKit|Card")
	FRPGKitCard Card;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Card")
	FRPGKitCard ToCard() const { return Card; }
};

// =========================================================================
//  GameMode
// =========================================================================

UCLASS()
class ARPGKitGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARPGKitGameMode();

	// ----- Blueprint-callable: setup -----

	// Reset fighters and effects to start a new encounter.
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	void SetupEncounter(
		const FRPGKitFighter& Hero,
		const FRPGKitFighter& Enemy);

	// ----- Blueprint-callable: cards -----

	// Deck of cards (configure in Blueprint defaults).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit|Cards")
	TArray<FRPGKitCard> CardPool;

	// Optional data-asset deck. If populated, DealHand draws from these instead
	// of CardPool so cards can be built as standalone editor assets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit|Cards")
	TArray<TObjectPtr<URPGKitCardDefinition>> CardDefinitions;

	// Cards currently in hand.
	UPROPERTY(BlueprintReadOnly, Category = "RPGKit|Cards")
	TArray<FRPGKitCard> CurrentHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit|Cards")
	int32 CurrentEnergy = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit|Cards")
	int32 MaxEnergy = 3;

	// Draw a new hand. Resets energy to max, refills hand from pool.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Cards")
	void DealHand(int32 NumCards = 5);

	// Play a card from hand by index. Returns false if unplayable.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Cards")
	bool PlayCard(int32 CardIndex);

	// Debug helper: returns one display line for a card in the current hand.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Cards|Debug")
	FString GetHandCardSummary(int32 CardIndex) const;

	// Debug helper: prints the current hand through OnCombatLog.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Cards|Debug")
	void LogCurrentHand();

	// Debug helper: logs the request, then calls PlayCard.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Cards|Debug")
	bool DebugPlayCard(int32 CardIndex);

	// Apply a persistent effect to the encounter (Tough Skin, Bleed, etc.).
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	bool ApplyEffect(URPGKitEffect* Effect);

	// Remove a persistent effect.
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	bool RemoveEffect(URPGKitEffect* Effect);

	// ----- Blueprint-callable: combat actions -----

	// Execute a strike from attacker to target with base damage.
	// Returns the chain breakdown (receipt).
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	FRPGKitChainResult Strike(
		const FString& AttackerId,
		const FString& TargetId,
		int32 BaseDamage);

	// Deal simple (unmodified) damage — bypasses chain.
	// Use for sources that should ignore modifiers (env damage, self-damage).
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	void DealRawDamage(const FString& TargetId, int32 Amount);

	// Apply block (shield) to a fighter.
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	void AddBlock(const FString& FighterId, int32 Amount);

	// End the current turn — publishes turn.ended, clears block.
	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	void EndTurn();

	UFUNCTION(BlueprintCallable, Category = "RPGKit")
	void EnemyTakeTurn();

	// ----- Blueprint-callable: queries -----

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit")
	const FRPGKitFighter& GetFighter(const FString& Id) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit")
	int32 GetTurnNumber() const { return TurnNumber; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Debug")
	const TArray<FString>& GetRecentCombatLog() const { return RecentCombatLog; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Debug")
	FString GetRecentCombatLogText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Debug")
	const TArray<FRPGKitChainStep>& GetLatestDamageBreakdown() const { return LatestDamageBreakdown; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Debug")
	FString GetLatestDamageBreakdownText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RPGKit|Debug")
	FString GetDamageBreakdownSummary(int32 StepIndex) const;

	// ----- Blueprint-implementable events (UI bindings) -----

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnCombatLog(const FString& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnDamageDealt(const FRPGKitChainResult& Result);

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnTurnEnded(int32 InTurnNumber);

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnFighterDied(const FString& FighterId);

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnEffectApplied(const FString& EffectId);

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnEffectRemoved(const FString& EffectId);

	// Fired after PlayCard() or DealHand() — Blueprint rebuilds card widgets.
	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit")
	void OnHandChanged();

	// ----- Accessors for C++ callers -----

	class rpg::core::Bus& GetBus();

private:
	friend struct FRPGKitActionExecutor;

	void EmitCombatLog(const FString& Message);
	void ClearAllBlock();
	FRPGKitFighter* FindFighter(const FString& Id);

	// Fighters keyed by ID.
	TMap<FString, FRPGKitFighter> Fighters;

	// Track applied effects for removal.
	UPROPERTY()
	TArray<URPGKitEffect*> ActiveEffects;

	// The bus is owned by the subsystem; cached pointer.
	UPROPERTY()
	TObjectPtr<URPGKitBus> BusSubsystem;

	rpg::core::SubscriptionId RawDamageSubscriptionId;

	UPROPERTY()
	TArray<FString> RecentCombatLog;

	UPROPERTY()
	TArray<FRPGKitChainStep> LatestDamageBreakdown;

	int32 TurnNumber = 0;
	int32 MaxRecentCombatLogLines = 8;
};
