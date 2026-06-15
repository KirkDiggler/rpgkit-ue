// Runtime owner for one combat encounter.

#pragma once

#include "CoreMinimal.h"
#include "RPGKitGameMode.h"
#include "RPGKitEncounterRuntime.generated.h"

class ARPGKitGameMode;
class URPGKitBus;
class URPGKitEffect;

UCLASS()
class URPGKitEncounterRuntime : public UObject
{
	GENERATED_BODY()

public:
	void SetupEncounter(ARPGKitGameMode* InHost, const FRPGKitFighter& Hero, const FRPGKitFighter& Enemy);
	void ShutdownEncounter();

	bool ApplyEffect(URPGKitEffect* Effect);
	bool RemoveEffect(URPGKitEffect* Effect);

	FRPGKitChainResult Strike(const FString& AttackerId, const FString& TargetId, int32 BaseDamage);
	void DealRawDamage(const FString& TargetId, int32 Amount);
	void AddBlock(const FString& FighterId, int32 Amount);
	void ClearAllBlock();

	const FRPGKitFighter& GetFighter(const FString& Id) const;
	FRPGKitFighter* FindFighter(const FString& Id);

	void EmitCombatLog(const FString& Message);

	class rpg::core::Bus& GetBus();

	int32 GetTurnNumber() const { return TurnNumber; }
	void SetTurnNumber(int32 InTurnNumber) { TurnNumber = InTurnNumber; }

	const TArray<FString>& GetRecentCombatLog() const { return RecentCombatLog; }
	const TArray<FRPGKitChainStep>& GetLatestDamageBreakdown() const { return LatestDamageBreakdown; }

private:
	void SubscribeEncounterRequests();
	void UnsubscribeEncounterRequests();

	rpg::core::Status HandleRawDamageRequest(const FRPGKitRawDamageRequest& Request);
	rpg::core::Status HandleBlockRequest(const FRPGKitBlockRequest& Request);

	UPROPERTY()
	TObjectPtr<ARPGKitGameMode> Host;

	UPROPERTY()
	TObjectPtr<URPGKitBus> BusSubsystem;

	TMap<FString, FRPGKitFighter> Fighters;

	UPROPERTY()
	TArray<URPGKitEffect*> ActiveEffects;

	rpg::core::SubscriptionId RawDamageSubscriptionId;
	rpg::core::SubscriptionId BlockSubscriptionId;

	UPROPERTY()
	TArray<FString> RecentCombatLog;

	UPROPERTY()
	TArray<FRPGKitChainStep> LatestDamageBreakdown;

	int32 TurnNumber = 0;
	int32 MaxRecentCombatLogLines = 8;
};
