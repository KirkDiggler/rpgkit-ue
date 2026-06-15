// rpgkit UE — GameMode implementation

#include "RPGKitGameMode.h"
#include "RPGKitActionExecutor.h"
#include "RPGKitBus.h"
#include "RPGKitEncounterRuntime.h"
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
	GetOrCreateEncounterRuntime()->SetupEncounter(this, Hero, Enemy);
}

bool ARPGKitGameMode::ApplyEffect(URPGKitEffect* Effect)
{
	return GetOrCreateEncounterRuntime()->ApplyEffect(Effect);
}

bool ARPGKitGameMode::RemoveEffect(URPGKitEffect* Effect)
{
	return GetOrCreateEncounterRuntime()->RemoveEffect(Effect);
}

FRPGKitChainResult ARPGKitGameMode::Strike(
	const FString& AttackerId,
	const FString& TargetId,
	int32 BaseDamage)
{
	return GetOrCreateEncounterRuntime()->Strike(AttackerId, TargetId, BaseDamage);
}

void ARPGKitGameMode::DealRawDamage(const FString& TargetId, int32 Amount)
{
	GetOrCreateEncounterRuntime()->DealRawDamage(TargetId, Amount);
}

void ARPGKitGameMode::AddBlock(const FString& FighterId, int32 Amount)
{
	GetOrCreateEncounterRuntime()->AddBlock(FighterId, Amount);
}

void ARPGKitGameMode::EndTurn()
{
	URPGKitEncounterRuntime* Runtime = GetOrCreateEncounterRuntime();
	Runtime->SetTurnNumber(Runtime->GetTurnNumber() + 1);

	// Publish turn.ended so subscribers react (bleed ticks, etc.).
	if (Runtime)
	{
		rpg::core::Topic<int32> topic =
			RPGKitTopics::kTurnEnded.on(Runtime->GetBus());
		(void)topic.publish(Runtime->GetTurnNumber());
	}

	OnTurnEnded(Runtime->GetTurnNumber());
	EmitCombatLog(FString::Printf(TEXT("--- Turn %d ---"), Runtime->GetTurnNumber()));

	EnemyTakeTurn();
	ClearAllBlock();
	DealHand(5);
}

void ARPGKitGameMode::ClearAllBlock()
{
	GetOrCreateEncounterRuntime()->ClearAllBlock();
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

	FRPGKitActionExecutor Executor;
	Executor.ExecuteAction(*this, Context, Attack);
}

const FRPGKitFighter& ARPGKitGameMode::GetFighter(const FString& Id) const
{
	static FRPGKitFighter Dummy;
	if (const URPGKitEncounterRuntime* Runtime = GetEncounterRuntime())
	{
		return Runtime->GetFighter(Id);
	}
	return Dummy;
}

FRPGKitFighter* ARPGKitGameMode::FindFighter(const FString& Id)
{
	return GetOrCreateEncounterRuntime()->FindFighter(Id);
}

void ARPGKitGameMode::EmitCombatLog(const FString& Message)
{
	GetOrCreateEncounterRuntime()->EmitCombatLog(Message);
}

rpg::core::Bus& ARPGKitGameMode::GetBus()
{
	return GetOrCreateEncounterRuntime()->GetBus();
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

	FRPGKitActionExecutor Executor;
	for (const FRPGKitCardAction& Action : Card.Actions)
	{
		Executor.ExecuteAction(*this, Context, Action);
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
	const TArray<FRPGKitChainStep>& DamageBreakdown = GetLatestDamageBreakdown();
	if (!DamageBreakdown.IsValidIndex(StepIndex))
	{
		return FString::Printf(TEXT("[%d] <empty>"), StepIndex);
	}

	const FRPGKitChainStep& Step = DamageBreakdown[StepIndex];
	return FString::Printf(TEXT("%s / %s: %d -> %d"),
		*Step.Stage,
		*Step.ModifierId,
		Step.Before,
		Step.After);
}

FString ARPGKitGameMode::GetRecentCombatLogText() const
{
	return FString::Join(GetRecentCombatLog(), TEXT("\n"));
}

FString ARPGKitGameMode::GetLatestDamageBreakdownText() const
{
	TArray<FString> Lines;
	const TArray<FRPGKitChainStep>& DamageBreakdown = GetLatestDamageBreakdown();
	Lines.Reserve(DamageBreakdown.Num());

	for (int32 Index = 0; Index < DamageBreakdown.Num(); ++Index)
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

int32 ARPGKitGameMode::GetTurnNumber() const
{
	if (const URPGKitEncounterRuntime* Runtime = GetEncounterRuntime())
	{
		return Runtime->GetTurnNumber();
	}
	return 0;
}

const TArray<FString>& ARPGKitGameMode::GetRecentCombatLog() const
{
	static const TArray<FString> Empty;
	if (const URPGKitEncounterRuntime* Runtime = GetEncounterRuntime())
	{
		return Runtime->GetRecentCombatLog();
	}
	return Empty;
}

const TArray<FRPGKitChainStep>& ARPGKitGameMode::GetLatestDamageBreakdown() const
{
	static const TArray<FRPGKitChainStep> Empty;
	if (const URPGKitEncounterRuntime* Runtime = GetEncounterRuntime())
	{
		return Runtime->GetLatestDamageBreakdown();
	}
	return Empty;
}

URPGKitEncounterRuntime* ARPGKitGameMode::GetOrCreateEncounterRuntime()
{
	if (!EncounterRuntime)
	{
		EncounterRuntime = NewObject<URPGKitEncounterRuntime>(this);
	}
	return EncounterRuntime;
}

const URPGKitEncounterRuntime* ARPGKitGameMode::GetEncounterRuntime() const
{
	return EncounterRuntime;
}
