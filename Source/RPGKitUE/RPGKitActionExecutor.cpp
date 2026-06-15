// Plain C++ action executor implementation.

#include "RPGKitActionExecutor.h"
#include "RPGKitEffect.h"
#include "RPGKitGameMode.h"

bool FRPGKitActionExecutor::ExecuteAction(
	ARPGKitGameMode& Runtime,
	const FRPGKitActionContext& Context,
	const FRPGKitCardAction& Action) const
{
	const FString TargetId = ResolveActionTargetId(Action, Context);

	switch (Action.Type)
	{
	case ERPGKitCardActionType::Damage:
		Runtime.Strike(Context.ActorId, TargetId, Action.Amount);
		return true;

	case ERPGKitCardActionType::Block:
		{
			FRPGKitBlockRequest Request;
			Request.SourceId = Context.ActorId;
			Request.TargetId = TargetId;
			Request.Amount = Action.Amount;

			rpg::core::Topic<FRPGKitBlockRequest> BlockTopic =
				RPGKitTopics::kBlockRequested.on(Runtime.GetBus());
			(void)BlockTopic.publish(Request);
		}
		return true;

	case ERPGKitCardActionType::Heal:
		if (FRPGKitFighter* Target = Runtime.FindFighter(TargetId))
		{
			Target->CurrentHP = FMath::Min(Target->MaxHP, Target->CurrentHP + Action.Amount);
			Runtime.EmitCombatLog(FString::Printf(TEXT("%s heals for %d HP → HP: %d/%d"),
				*Target->Name, Action.Amount, Target->CurrentHP, Target->MaxHP));
			return true;
		}
		return false;

	case ERPGKitCardActionType::ApplyBleed:
		{
			URPGKitBleedEffect* Bleed = NewObject<URPGKitBleedEffect>(&Runtime);
			Bleed->TargetEntityId = TargetId;
			Bleed->Stacks = Action.Amount > 0 ? Action.Amount : 3;
			Bleed->DamagePerStack = Action.DurationTurns > 0 ? Action.DurationTurns : 2;

			if (Runtime.ApplyEffect(Bleed))
			{
				const FRPGKitFighter& Target = Runtime.GetFighter(TargetId);
				Runtime.EmitCombatLog(FString::Printf(TEXT("%s is Bleeding (%d stacks, %d damage per stack)."),
					*Target.Name, Bleed->Stacks, Bleed->DamagePerStack));
				return true;
			}
			return false;
		}

	case ERPGKitCardActionType::ApplyVulnerable:
		{
			URPGKitVulnerableEffect* Vulnerable = NewObject<URPGKitVulnerableEffect>(&Runtime);
			Vulnerable->TargetEntityId = TargetId;
			Vulnerable->PercentBonus = Action.Amount > 0 ? Action.Amount : 50;
			Vulnerable->RemainingTurns = Action.DurationTurns > 0 ? Action.DurationTurns : 2;

			if (Runtime.ApplyEffect(Vulnerable))
			{
				const FRPGKitFighter& Target = Runtime.GetFighter(TargetId);
				Runtime.EmitCombatLog(FString::Printf(TEXT("%s is Vulnerable (+%d%% damage) for %d turns."),
					*Target.Name, Vulnerable->PercentBonus, Vulnerable->RemainingTurns));
				return true;
			}
			return false;
		}
	}

	return false;
}

FString FRPGKitActionExecutor::ResolveActionTargetId(
	const FRPGKitCardAction& Action,
	const FRPGKitActionContext& Context) const
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
