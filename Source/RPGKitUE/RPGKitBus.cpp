// rpgkit UE — Bus subsystem implementation

#include "RPGKitBus.h"
#include "RPGKitEffect.h"

#include "rpg/core/bus.hpp"

void URPGKitBus::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RawBus = MakeUnique<rpg::core::Bus>();
}

void URPGKitBus::Deinitialize()
{
	RawBus.Reset();
	Super::Deinitialize();
}

void URPGKitBus::PublishEvent(FName TopicId, const FRPGKitEventPayload& Payload)
{
	if (!RawBus) return;

	FString TopicStr = TopicId.ToString();

	if (TopicStr == RPGKitTopics::kTurnEnded.id().c_str())
	{
		rpg::core::Topic<int32> topic = RPGKitTopics::kTurnEnded.on(*RawBus);
		(void)topic.publish(Payload.IntValue);
	}
}

FRPGKitChainResult URPGKitBus::ExecuteDamageChain(const FRPGKitDamageEvent& Event)
{
	FRPGKitChainResult Result;
	if (!RawBus) return Result;

	// Build a fresh chain per resolution.
	rpg::core::Chain<FRPGKitDamageEvent> chain(RPGKitTopics::kDamageStages);

	// Publish collects subscriber contributions into the chain.
	rpg::core::ChainedTopic<FRPGKitDamageEvent> topic =
		RPGKitTopics::kCombatDamage.onChained(*RawBus);
	rpg::core::Status status = topic.publish(Event, chain);
	if (!status.isOk())
	{
		UE_LOG(LogTemp, Warning, TEXT("RPGKit: chain publish failed: %s"),
			UTF8_TO_TCHAR(status.message().c_str()));
		return Result;
	}

	// Execute applies the collected modifiers.
	auto chainResult = chain.execute(Event);
	Result.Value = chainResult.value.BaseAmount;

	for (const auto& step : chainResult.breakdown)
	{
		FRPGKitChainStep S;
		S.Stage = UTF8_TO_TCHAR(step.stage.c_str());
		S.ModifierId = UTF8_TO_TCHAR(step.id.c_str());
		S.Before = step.before.BaseAmount;
		S.After = step.after.BaseAmount;
		Result.Breakdown.Add(S);
	}

	return Result;
}

bool URPGKitBus::ApplyEffect(URPGKitEffect* Effect)
{
	if (!RawBus || !Effect) return false;
	rpg::core::Status status = Effect->GetRawEffect().apply(*RawBus);
	if (!status.isOk())
	{
		UE_LOG(LogTemp, Warning, TEXT("RPGKit: apply effect failed: %s"),
			UTF8_TO_TCHAR(status.message().c_str()));
		return false;
	}
	return true;
}

bool URPGKitBus::RemoveEffect(URPGKitEffect* Effect)
{
	if (!RawBus || !Effect) return false;
	rpg::core::Status status = Effect->GetRawEffect().remove();
	if (!status.isOk())
	{
		UE_LOG(LogTemp, Warning, TEXT("RPGKit: remove effect failed: %s"),
			UTF8_TO_TCHAR(status.message().c_str()));
		return false;
	}
	return true;
}
