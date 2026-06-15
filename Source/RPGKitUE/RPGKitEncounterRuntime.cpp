// Runtime owner for one combat encounter.

#include "RPGKitEncounterRuntime.h"
#include "RPGKitBus.h"
#include "RPGKitEffect.h"
#include "Engine/GameInstance.h"

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/topic.hpp"

void URPGKitEncounterRuntime::SetupEncounter(ARPGKitGameMode* InHost, const FRPGKitFighter& Hero, const FRPGKitFighter& Enemy)
{
	ShutdownEncounter();

	Host = InHost;
	Fighters.Empty();
	ActiveEffects.Empty();
	RecentCombatLog.Empty();
	LatestDamageBreakdown.Empty();
	TurnNumber = 0;

	Fighters.Add("hero", Hero);
	Fighters.Add("goblin", Enemy);

	if (Host)
	{
		if (UGameInstance* GI = Host->GetGameInstance())
		{
			BusSubsystem = GI->GetSubsystem<URPGKitBus>();
		}
	}

	SubscribeEncounterRequests();

	EmitCombatLog(FString::Printf(TEXT("=== Encounter: %s vs %s ==="), *Hero.Name, *Enemy.Name));
}

void URPGKitEncounterRuntime::ShutdownEncounter()
{
	UnsubscribeEncounterRequests();
	ActiveEffects.Empty();
}

void URPGKitEncounterRuntime::SubscribeEncounterRequests()
{
	if (!BusSubsystem)
	{
		return;
	}

	rpg::core::Topic<FRPGKitRawDamageRequest> RawDamageTopic =
		RPGKitTopics::kRawDamageRequested.on(BusSubsystem->GetRawBus());

	RawDamageSubscriptionId = RawDamageTopic.subscribe([this](const FRPGKitRawDamageRequest& Request) -> rpg::core::Status {
		return HandleRawDamageRequest(Request);
	});

	rpg::core::Topic<FRPGKitBlockRequest> BlockTopic =
		RPGKitTopics::kBlockRequested.on(BusSubsystem->GetRawBus());

	BlockSubscriptionId = BlockTopic.subscribe([this](const FRPGKitBlockRequest& Request) -> rpg::core::Status {
		return HandleBlockRequest(Request);
	});
}

void URPGKitEncounterRuntime::UnsubscribeEncounterRequests()
{
	if (BusSubsystem && RawDamageSubscriptionId.value != 0)
	{
		(void)BusSubsystem->GetRawBus().unsubscribe(RawDamageSubscriptionId);
		RawDamageSubscriptionId = rpg::core::SubscriptionId{};
	}

	if (BusSubsystem && BlockSubscriptionId.value != 0)
	{
		(void)BusSubsystem->GetRawBus().unsubscribe(BlockSubscriptionId);
		BlockSubscriptionId = rpg::core::SubscriptionId{};
	}
}

rpg::core::Status URPGKitEncounterRuntime::HandleRawDamageRequest(const FRPGKitRawDamageRequest& Request)
{
	EmitCombatLog(FString::Printf(TEXT("%s requests %d raw damage to %s."),
		*Request.SourceId, Request.Amount, *Request.TargetId));
	DealRawDamage(Request.TargetId, Request.Amount);
	return rpg::core::Status::ok();
}

rpg::core::Status URPGKitEncounterRuntime::HandleBlockRequest(const FRPGKitBlockRequest& Request)
{
	EmitCombatLog(FString::Printf(TEXT("%s requests %d block to %s."),
		*Request.SourceId, Request.Amount, *Request.TargetId));
	AddBlock(Request.TargetId, Request.Amount);
	return rpg::core::Status::ok();
}

bool URPGKitEncounterRuntime::ApplyEffect(URPGKitEffect* Effect)
{
	if (!BusSubsystem || !Effect) return false;

	if (ActiveEffects.Contains(Effect))
	{
		RemoveEffect(Effect);
	}

	bool bSuccess = BusSubsystem->ApplyEffect(Effect);
	if (bSuccess)
	{
		ActiveEffects.Add(Effect);
		Effect->OnEffectApplied();
		if (Host)
		{
			Host->OnEffectApplied(Effect->GetName());
		}
	}
	return bSuccess;
}

bool URPGKitEncounterRuntime::RemoveEffect(URPGKitEffect* Effect)
{
	if (!BusSubsystem || !Effect) return false;

	bool bSuccess = BusSubsystem->RemoveEffect(Effect);
	if (bSuccess)
	{
		ActiveEffects.Remove(Effect);
		Effect->OnEffectRemoved();
		if (Host)
		{
			Host->OnEffectRemoved(Effect->GetName());
		}
	}
	return bSuccess;
}

FRPGKitChainResult URPGKitEncounterRuntime::Strike(const FString& AttackerId, const FString& TargetId, int32 BaseDamage)
{
	if (!BusSubsystem)
	{
		return FRPGKitChainResult();
	}

	FRPGKitDamageEvent Event;
	Event.AttackerId = AttackerId;
	Event.TargetId = TargetId;
	Event.BaseAmount = BaseDamage;

	FRPGKitChainResult Result = BusSubsystem->ExecuteDamageChain(Event);
	LatestDamageBreakdown = Result.Breakdown;

	int32 FinalDamage = Result.Value;

	if (FRPGKitFighter* Target = FindFighter(TargetId))
	{
		int32 Blocked = FMath::Min(Target->Block, FinalDamage);
		Target->Block -= Blocked;
		int32 HPDamage = FinalDamage - Blocked;
		Target->CurrentHP = FMath::Max(0, Target->CurrentHP - HPDamage);

		if (Host)
		{
			Host->OnDamageDealt(Result);
		}

		if (Blocked > 0)
		{
			EmitCombatLog(FString::Printf(
				TEXT("%s blocks %d damage! (%d gets through)"),
				*Target->Name, Blocked, HPDamage));
		}

		EmitCombatLog(FString::Printf(
			TEXT("%s strikes %s for %d damage → %s HP: %d/%d"),
			*AttackerId, *TargetId, FinalDamage,
			*Target->Name, Target->CurrentHP, Target->MaxHP));

		if (!Target->IsAlive() && Host)
		{
			Host->OnFighterDied(TargetId);
		}
	}

	return Result;
}

void URPGKitEncounterRuntime::DealRawDamage(const FString& TargetId, int32 Amount)
{
	if (FRPGKitFighter* Target = FindFighter(TargetId))
	{
		Target->CurrentHP = FMath::Max(0, Target->CurrentHP - Amount);

		EmitCombatLog(FString::Printf(
			TEXT("%s takes %d raw damage → HP: %d/%d"),
			*Target->Name, Amount, Target->CurrentHP, Target->MaxHP));

		if (!Target->IsAlive() && Host)
		{
			Host->OnFighterDied(TargetId);
		}
	}
}

void URPGKitEncounterRuntime::AddBlock(const FString& FighterId, int32 Amount)
{
	if (FRPGKitFighter* Fighter = FindFighter(FighterId))
	{
		Fighter->Block += Amount;
		EmitCombatLog(FString::Printf(
			TEXT("%s gains %d block (total: %d)"),
			*Fighter->Name, Amount, Fighter->Block));
	}
}

void URPGKitEncounterRuntime::ClearAllBlock()
{
	for (auto& Pair : Fighters)
	{
		if (Pair.Value.Block > 0)
		{
			EmitCombatLog(FString::Printf(
				TEXT("%s's block expires."), *Pair.Value.Name));
		}
		Pair.Value.Block = 0;
	}
}

const FRPGKitFighter& URPGKitEncounterRuntime::GetFighter(const FString& Id) const
{
	static FRPGKitFighter Dummy;
	if (const FRPGKitFighter* Found = Fighters.Find(Id))
	{
		return *Found;
	}
	return Dummy;
}

FRPGKitFighter* URPGKitEncounterRuntime::FindFighter(const FString& Id)
{
	return Fighters.Find(Id);
}

void URPGKitEncounterRuntime::EmitCombatLog(const FString& Message)
{
	RecentCombatLog.Add(Message);
	while (RecentCombatLog.Num() > MaxRecentCombatLogLines)
	{
		RecentCombatLog.RemoveAt(0);
	}

	if (Host)
	{
		Host->OnCombatLog(Message);
	}
}

rpg::core::Bus& URPGKitEncounterRuntime::GetBus()
{
	check(BusSubsystem);
	return BusSubsystem->GetRawBus();
}
