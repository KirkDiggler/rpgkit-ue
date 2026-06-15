// UE wrapper around rpg::core::Effect. Blueprintable base class for
// persistent effects (Bleed, Rage, Tough Skin, etc.).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "rpg/core/effect.hpp"
#include "rpg/core/bus.hpp"

#include "RPGKitEffect.generated.h"

struct FRPGKitDamageEvent;

// Abstract base for all rpgkit effects in UE.
// Subclasses own an inner rpg::core::Effect that is applied/removed via the Bus.
UCLASS(Abstract, BlueprintType, Blueprintable)
class URPGKitEffect : public UObject
{
	GENERATED_BODY()

public:
	// The raw rpg::core::Effect — set by subclasses in their constructor.
	rpg::core::Effect& GetRawEffect() { check(RawEffectPtr); return *RawEffectPtr; }

	// Blueprint hooks for notification.
	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit|Effect")
	void OnEffectApplied();

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit|Effect")
	void OnEffectRemoved();

protected:
	rpg::core::Effect* RawEffectPtr = nullptr;
};

// =========================================================================
//  Tutorial 07: Tough Skin — passive damage reduction
// =========================================================================

UCLASS(BlueprintType)
class URPGKitToughSkinEffect : public URPGKitEffect
{
	GENERATED_BODY()

public:
	URPGKitToughSkinEffect();
	virtual ~URPGKitToughSkinEffect() override;

	// Entity this effect protects (usually "goblin").
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString ProtectedEntityId = "goblin";

	// Flat damage reduction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 DamageReduction = 1;

private:
	class FToughSkinEffect;
	FToughSkinEffect* InnerEffectImpl = nullptr;
};

// =========================================================================
//  Tutorial 08: Bleed — stacking DoT applied by the Rend card
// =========================================================================

UCLASS(BlueprintType)
class URPGKitBleedEffect : public URPGKitEffect
{
	GENERATED_BODY()

public:
	URPGKitBleedEffect();
	virtual ~URPGKitBleedEffect() override;

	// Entity that takes the bleed damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString TargetEntityId = "hero";

	// Damage dealt per stack each turn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 DamagePerStack = 2;

	// Number of stacks remaining.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 Stacks = 3;

	// Called each turn the bleed ticks (Blueprints can bind to the Stacks property change).
	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit|Effect")
	void OnBleedTicked(int32 RemainingStacks, int32 DamageDealt);

	// Tracked subscription IDs — managed internally.
	TArray<uint64> TrackedSubscriptionIds;

private:
	class FBleedEffect;
	FBleedEffect* InnerEffectImpl = nullptr;
};

// =========================================================================
//  Vulnerable — incoming damage multiplier with turn duration
// =========================================================================

UCLASS(BlueprintType)
class URPGKitVulnerableEffect : public URPGKitEffect
{
	GENERATED_BODY()

public:
	URPGKitVulnerableEffect();
	virtual ~URPGKitVulnerableEffect() override;

	// Entity that takes increased damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	FString TargetEntityId = "goblin";

	// Percent bonus to incoming damage, e.g. 50 means +50%.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 PercentBonus = 50;

	// Number of turn.ended events before this effect stops contributing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPGKit")
	int32 RemainingTurns = 2;

	UFUNCTION(BlueprintImplementableEvent, Category = "RPGKit|Effect")
	void OnVulnerableTicked(int32 InRemainingTurns);

private:
	class FVulnerableEffect;
	FVulnerableEffect* InnerEffectImpl = nullptr;
};
