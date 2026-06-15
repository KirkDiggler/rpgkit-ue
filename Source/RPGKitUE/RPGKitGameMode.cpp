// rpgkit UE — GameMode implementation

#include "RPGKitGameMode.h"
#include "RPGKitBus.h"
#include "RPGKitEffect.h"
#include "Engine/GameInstance.h"

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/topic.hpp"

ARPGKitGameMode::ARPGKitGameMode()
{
}

void ARPGKitGameMode::SetupEncounter(const FRPGKitFighter& Hero, const FRPGKitFighter& Enemy)
{
	if (BusSubsystem && RawDamageSubscriptionId.value != 0)
	{
		(void)BusSubsystem->GetRawBus().unsubscribe(RawDamageSubscriptionId);
		RawDamageSubscriptionId = rpg::core::SubscriptionId{};
	}

	Fighters.Empty();
	ActiveEffects.Empty();
	RecentCombatLog.Empty();
	TurnNumber = 0;

	Fighters.Add("hero", Hero);
	Fighters.Add("goblin", Enemy);

	// Cache the bus subsystem.
	if (UGameInstance* GI = GetGameInstance())
	{
		BusSubsystem = GI->GetSubsystem<URPGKitBus>();
	}

	if (BusSubsystem)
	{
		rpg::core::Topic<FRPGKitRawDamageRequest> rawDamageTopic =
			RPGKitTopics::kRawDamageRequested.on(BusSubsystem->GetRawBus());

		RawDamageSubscriptionId = rawDamageTopic.subscribe([this](const FRPGKitRawDamageRequest& Request) -> rpg::core::Status {
			EmitCombatLog(FString::Printf(TEXT("%s requests %d raw damage to %s."),
				*Request.SourceId, Request.Amount, *Request.TargetId));
			DealRawDamage(Request.TargetId, Request.Amount);
			return rpg::core::Status::ok();
		});
	}

	EmitCombatLog(FString::Printf(TEXT("=== Encounter: %s vs %s ==="), *Hero.Name, *Enemy.Name));
}

bool ARPGKitGameMode::ApplyEffect(URPGKitEffect* Effect)
{
	if (!BusSubsystem || !Effect) return false;

	// If already active, remove first.
	if (ActiveEffects.Contains(Effect))
	{
		RemoveEffect(Effect);
	}

	bool bSuccess = BusSubsystem->ApplyEffect(Effect);
	if (bSuccess)
	{
		ActiveEffects.Add(Effect);
		Effect->OnEffectApplied();
		OnEffectApplied(Effect->GetName());
	}
	return bSuccess;
}

bool ARPGKitGameMode::RemoveEffect(URPGKitEffect* Effect)
{
	if (!BusSubsystem || !Effect) return false;

	bool bSuccess = BusSubsystem->RemoveEffect(Effect);
	if (bSuccess)
	{
		ActiveEffects.Remove(Effect);
		Effect->OnEffectRemoved();
		OnEffectRemoved(Effect->GetName());
	}
	return bSuccess;
}

FRPGKitChainResult ARPGKitGameMode::Strike(
	const FString& AttackerId,
	const FString& TargetId,
	int32 BaseDamage)
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

	// Apply damage to target.
	if (FRPGKitFighter* Target = FindFighter(TargetId))
	{
		int32 Blocked = FMath::Min(Target->Block, FinalDamage);
		Target->Block -= Blocked;
		int32 HPDamage = FinalDamage - Blocked;
		Target->CurrentHP = FMath::Max(0, Target->CurrentHP - HPDamage);

		OnDamageDealt(Result);

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

		if (!Target->IsAlive())
		{
			OnFighterDied(TargetId);
		}
	}

	return Result;
}

void ARPGKitGameMode::DealRawDamage(const FString& TargetId, int32 Amount)
{
	if (FRPGKitFighter* Target = FindFighter(TargetId))
	{
		Target->CurrentHP = FMath::Max(0, Target->CurrentHP - Amount);

		EmitCombatLog(FString::Printf(
			TEXT("%s takes %d raw damage → HP: %d/%d"),
			*Target->Name, Amount, Target->CurrentHP, Target->MaxHP));

		if (!Target->IsAlive())
		{
			OnFighterDied(TargetId);
		}
	}
}

void ARPGKitGameMode::AddBlock(const FString& FighterId, int32 Amount)
{
	if (FRPGKitFighter* Fighter = FindFighter(FighterId))
	{
		Fighter->Block += Amount;
		EmitCombatLog(FString::Printf(
			TEXT("%s gains %d block (total: %d)"),
			*Fighter->Name, Amount, Fighter->Block));
	}
}

void ARPGKitGameMode::EndTurn()
{
	TurnNumber++;

	// Publish turn.ended so subscribers react (bleed ticks, etc.).
	if (BusSubsystem)
	{
		rpg::core::Topic<int32> topic =
			RPGKitTopics::kTurnEnded.on(BusSubsystem->GetRawBus());
		(void)topic.publish(TurnNumber);
	}

	OnTurnEnded(TurnNumber);
	EmitCombatLog(FString::Printf(TEXT("--- Turn %d ---"), TurnNumber));

	EnemyTakeTurn();
	ClearAllBlock();
	DealHand(5);
}

void ARPGKitGameMode::ClearAllBlock()
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

void ARPGKitGameMode::EnemyTakeTurn()
{
	const FRPGKitFighter& Goblin = GetFighter(TEXT("goblin"));
	const FRPGKitFighter& Hero = GetFighter(TEXT("hero"));
	if (!Goblin.IsAlive() || !Hero.IsAlive())
	{
		return;
	}

	EmitCombatLog(FString::Printf(TEXT("%s attacks!"), *Goblin.Name));

	FRPGKitActionContext Context;
	Context.ActorId = TEXT("goblin");
	Context.EnemyId = TEXT("hero");
	Context.SourceName = TEXT("Goblin Attack");

	FRPGKitCardAction Attack;
	Attack.Type = ERPGKitCardActionType::Damage;
	Attack.Amount = 6;
	Attack.Target = ERPGKitActionTargetMode::Enemy;

	ExecuteCardAction(Context, Attack);
}

const FRPGKitFighter& ARPGKitGameMode::GetFighter(const FString& Id) const
{
	static FRPGKitFighter Dummy;
	if (const FRPGKitFighter* Found = Fighters.Find(Id))
	{
		return *Found;
	}
	return Dummy;
}

FRPGKitFighter* ARPGKitGameMode::FindFighter(const FString& Id)
{
	return Fighters.Find(Id);
}

void ARPGKitGameMode::EmitCombatLog(const FString& Message)
{
	RecentCombatLog.Add(Message);
	while (RecentCombatLog.Num() > MaxRecentCombatLogLines)
	{
		RecentCombatLog.RemoveAt(0);
	}

	OnCombatLog(Message);
}

rpg::core::Bus& ARPGKitGameMode::GetBus()
{
	check(BusSubsystem);
	return BusSubsystem->GetRawBus();
}

// =========================================================================
//  Card system
// =========================================================================

void ARPGKitGameMode::DealHand(int32 NumCards)
{
	CurrentHand.Empty();
	CurrentEnergy = MaxEnergy;

	if (CardDefinitions.Num() > 0)
	{
		TArray<URPGKitCardDefinition*> ValidDefinitions;
		for (URPGKitCardDefinition* Definition : CardDefinitions)
		{
			if (Definition)
			{
				ValidDefinitions.Add(Definition);
			}
		}

		if (ValidDefinitions.Num() > 0)
		{
			EmitCombatLog(FString::Printf(TEXT("Dealing %d cards from %d card definitions."), NumCards, ValidDefinitions.Num()));

			for (int32 i = 0; i < NumCards; ++i)
			{
				const int32 Idx = FMath::RandRange(0, ValidDefinitions.Num() - 1);
				const FRPGKitCard Card = ValidDefinitions[Idx]->ToCard();
				CurrentHand.Add(Card);
				EmitCombatLog(FString::Printf(TEXT("  [%d] %s (cost %d)"), i, *Card.Name, Card.Cost));
			}

			OnHandChanged();
			return;
		}
	}

	if (CardPool.Num() == 0)
	{
		EmitCombatLog(TEXT("Card pool is empty — add cards or card definitions in BP_RPGKitGameMode defaults."));
		return;
	}

	for (int32 i = 0; i < NumCards; ++i)
	{
		int32 Idx = FMath::RandRange(0, CardPool.Num() - 1);
		const FRPGKitCard Card = CardPool[Idx];
		CurrentHand.Add(Card);
		EmitCombatLog(FString::Printf(TEXT("  [%d] %s (cost %d)"), i, *Card.Name, Card.Cost));
	}

	OnHandChanged();
}

bool ARPGKitGameMode::PlayCard(int32 CardIndex)
{
	if (!CurrentHand.IsValidIndex(CardIndex))
	{
		EmitCombatLog(FString::Printf(TEXT("Cannot play card index %d. Current hand has %d cards."), CardIndex, CurrentHand.Num()));
		return false;
	}

	const FRPGKitCard& Card = CurrentHand[CardIndex];
	EmitCombatLog(FString::Printf(TEXT("Playing [%d] %s."), CardIndex, *Card.Name));

	if (Card.Cost > CurrentEnergy)
	{
		EmitCombatLog(FString::Printf(TEXT("Not enough energy! (%d needed, %d available)"), Card.Cost, CurrentEnergy));
		return false;
	}

	CurrentEnergy -= Card.Cost;

	FRPGKitActionContext Context;
	Context.ActorId = TEXT("hero");
	Context.EnemyId = TEXT("goblin");
	Context.SourceName = Card.Name;

	if (Card.Actions.Num() == 0)
	{
		EmitCombatLog(FString::Printf(TEXT("%s has no actions."), *Card.Name));
	}

	for (const FRPGKitCardAction& Action : Card.Actions)
	{
		ExecuteCardAction(Context, Action);
	}

	// Remove card from hand.
	CurrentHand.RemoveAt(CardIndex);

	// Check for victory.
	if (FRPGKitFighter* Goblin = FindFighter("goblin"))
	{
		if (!Goblin->IsAlive())
		{
			EmitCombatLog(FString::Printf(TEXT("Victory! %s has been defeated!"), *Goblin->Name));
		}
	}

	// Notify Blueprint UI to refresh.
	OnHandChanged();

	return true;
}

bool ARPGKitGameMode::ExecuteCardAction(const FRPGKitActionContext& Context, const FRPGKitCardAction& Action)
{
	const FString TargetId = ResolveActionTargetId(Action, Context);

	switch (Action.Type)
	{
	case ERPGKitCardActionType::Damage:
		Strike(Context.ActorId, TargetId, Action.Amount);
		return true;

	case ERPGKitCardActionType::Block:
		AddBlock(TargetId, Action.Amount);
		return true;

	case ERPGKitCardActionType::Heal:
		if (FRPGKitFighter* Target = FindFighter(TargetId))
		{
			Target->CurrentHP = FMath::Min(Target->MaxHP, Target->CurrentHP + Action.Amount);
			EmitCombatLog(FString::Printf(TEXT("%s heals for %d HP → HP: %d/%d"),
				*Target->Name, Action.Amount, Target->CurrentHP, Target->MaxHP));
			return true;
		}
		return false;

	case ERPGKitCardActionType::ApplyBleed:
		{
			URPGKitBleedEffect* Bleed = NewObject<URPGKitBleedEffect>(this);
			Bleed->TargetEntityId = TargetId;
			Bleed->Stacks = Action.Amount > 0 ? Action.Amount : 3;
			Bleed->DamagePerStack = Action.DurationTurns > 0 ? Action.DurationTurns : 2;

			if (ApplyEffect(Bleed))
			{
				const FRPGKitFighter& Target = GetFighter(TargetId);
				EmitCombatLog(FString::Printf(TEXT("%s is Bleeding (%d stacks, %d damage per stack)."),
					*Target.Name, Bleed->Stacks, Bleed->DamagePerStack));
				return true;
			}
			return false;
		}

	case ERPGKitCardActionType::ApplyVulnerable:
		{
			URPGKitVulnerableEffect* Vulnerable = NewObject<URPGKitVulnerableEffect>(this);
			Vulnerable->TargetEntityId = TargetId;
			Vulnerable->PercentBonus = Action.Amount > 0 ? Action.Amount : 50;
			Vulnerable->RemainingTurns = Action.DurationTurns > 0 ? Action.DurationTurns : 2;

			if (ApplyEffect(Vulnerable))
			{
				const FRPGKitFighter& Target = GetFighter(TargetId);
				EmitCombatLog(FString::Printf(TEXT("%s is Vulnerable (+%d%% damage) for %d turns."),
					*Target.Name, Vulnerable->PercentBonus, Vulnerable->RemainingTurns));
				return true;
			}
			return false;
		}
	}

	return false;
}

FString ARPGKitGameMode::ResolveActionTargetId(const FRPGKitCardAction& Action, const FRPGKitActionContext& Context) const
{
	switch (Action.Target)
	{
	case ERPGKitActionTargetMode::Self:
		return Context.ActorId;

	case ERPGKitActionTargetMode::Enemy:
		return Context.EnemyId;

	case ERPGKitActionTargetMode::Explicit:
		return Action.ExplicitTargetId.IsEmpty() ? Context.EnemyId : Action.ExplicitTargetId;
	}

	return Context.EnemyId;
}

FString ARPGKitGameMode::GetHandCardSummary(int32 CardIndex) const
{
	if (!CurrentHand.IsValidIndex(CardIndex))
	{
		return TEXT("");
	}

	const FRPGKitCard& Card = CurrentHand[CardIndex];

	TArray<FString> EffectLines;
	for (const FRPGKitCardAction& Action : Card.Actions)
	{
		switch (Action.Type)
		{
		case ERPGKitCardActionType::Damage:
			EffectLines.Add(FString::Printf(TEXT("Deal %d damage"), Action.Amount));
			break;

		case ERPGKitCardActionType::Block:
			EffectLines.Add(FString::Printf(TEXT("Gain %d block"), Action.Amount));
			break;

		case ERPGKitCardActionType::Heal:
			EffectLines.Add(FString::Printf(TEXT("Heal %d HP"), Action.Amount));
			break;

		case ERPGKitCardActionType::ApplyBleed:
			EffectLines.Add(FString::Printf(TEXT("Apply %d Bleed\n%d damage/stack"),
				Action.Amount,
				Action.DurationTurns));
			break;

		case ERPGKitCardActionType::ApplyVulnerable:
			EffectLines.Add(FString::Printf(TEXT("Apply Vulnerable\n+%d%% damage, %d turns"),
				Action.Amount,
				Action.DurationTurns));
			break;
		}
	}

	const FString EffectsText = EffectLines.Num() > 0
		? FString::Join(EffectLines, TEXT("\n"))
		: TEXT("No effect");

	return FString::Printf(TEXT("%s\nCost: %d\n%s"),
		Card.Name.IsEmpty() ? TEXT("<unnamed>") : *Card.Name,
		Card.Cost,
		*EffectsText);
}

FString ARPGKitGameMode::GetDamageBreakdownSummary(int32 StepIndex) const
{
	if (!LatestDamageBreakdown.IsValidIndex(StepIndex))
	{
		return FString::Printf(TEXT("[%d] <empty>"), StepIndex);
	}

	const FRPGKitChainStep& Step = LatestDamageBreakdown[StepIndex];
	return FString::Printf(TEXT("%s / %s: %d -> %d"),
		*Step.Stage,
		*Step.ModifierId,
		Step.Before,
		Step.After);
}

FString ARPGKitGameMode::GetRecentCombatLogText() const
{
	return FString::Join(RecentCombatLog, TEXT("\n"));
}

FString ARPGKitGameMode::GetLatestDamageBreakdownText() const
{
	TArray<FString> Lines;
	Lines.Reserve(LatestDamageBreakdown.Num());

	for (int32 Index = 0; Index < LatestDamageBreakdown.Num(); ++Index)
	{
		Lines.Add(GetDamageBreakdownSummary(Index));
	}

	return FString::Join(Lines, TEXT("\n"));
}

void ARPGKitGameMode::LogCurrentHand()
{
	EmitCombatLog(FString::Printf(TEXT("Current hand: %d card(s), energy %d/%d"),
		CurrentHand.Num(), CurrentEnergy, MaxEnergy));

	for (int32 Index = 0; Index < CurrentHand.Num(); ++Index)
	{
		EmitCombatLog(GetHandCardSummary(Index));
	}
}

bool ARPGKitGameMode::DebugPlayCard(int32 CardIndex)
{
	EmitCombatLog(FString::Printf(TEXT("DebugPlayCard(%d)"), CardIndex));
	return PlayCard(CardIndex);
}
