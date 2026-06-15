// Blueprint-accessible wrapper around rpg::core::Bus.
// Owns the event bus; game code subscribes typed topics through it.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "rpg/core/topic.hpp"
#include "rpg/core/chain.hpp"

#include "RPGKitBus.generated.h"

// Simple payload struct for notification events.
// For rich events (tutorials 07-08), Blueprint subclasses can cast to typed payloads.
USTRUCT(BlueprintType)
struct FRPGKitEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FName TopicId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 IntValue = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString StringValue;
};

// Damage event for combat (tutorials 06-08).
USTRUCT(BlueprintType)
struct FRPGKitDamageEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString AttackerId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString TargetId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 BaseAmount = 0;
};

USTRUCT(BlueprintType)
struct FRPGKitRawDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString SourceId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString TargetId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 Amount = 0;
};

USTRUCT(BlueprintType)
struct FRPGKitBlockRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString SourceId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString TargetId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 Amount = 0;
};

// Single step in a chain breakdown: "(stage) id: before → after".
USTRUCT(BlueprintType)
struct FRPGKitChainStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString Stage;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	FString ModifierId;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 Before = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 After = 0;
};

// Result of executing a chain.
USTRUCT(BlueprintType)
struct FRPGKitChainResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	int32 Value = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RPGKit")
	TArray<FRPGKitChainStep> Breakdown;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPGKitEvent, const FRPGKitEventPayload&, Payload);

// Shared topic definitions — declared once, bound to any Bus.
// Both the Bus subsystem and Effect classes reference these.
namespace RPGKitTopics
{
	inline rpg::core::TopicDef<int32> kTurnEnded("turn.ended");
	inline rpg::core::TopicDef<FRPGKitDamageEvent> kCombatDamage("combat.damage");
	inline rpg::core::TopicDef<FRPGKitRawDamageRequest> kRawDamageRequested("combat.raw_damage.requested");
	inline rpg::core::TopicDef<FRPGKitBlockRequest> kBlockRequested("combat.block.requested");
	inline std::vector<std::string> kDamageStages = {"base", "effects", "final"};
}

// Module-level subsystem: one Bus per game session.
// Wire up topics in C++ or Blueprint; publish events from anywhere.
UCLASS()
class URPGKitBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ----- Blueprint-callable helpers -----

	// Fire a notification event on a topic.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Bus")
	void PublishEvent(FName TopicId, const FRPGKitEventPayload& Payload);

	// Execute a chain for a damage event (tutorial 06-08: rich events).
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Bus")
	FRPGKitChainResult ExecuteDamageChain(const FRPGKitDamageEvent& Event);

	// Apply/remove a persistent effect on the bus.  The caller manages the
	// UObject lifetime; these helpers register/unregister subscriptions on
	// the internal rpg::core::Bus.
	UFUNCTION(BlueprintCallable, Category = "RPGKit|Bus")
	bool ApplyEffect(class URPGKitEffect* Effect);

	UFUNCTION(BlueprintCallable, Category = "RPGKit|Bus")
	bool RemoveEffect(class URPGKitEffect* Effect);

	// Direct access for C++ callers that want the raw Bus.
	class rpg::core::Bus& GetRawBus() { return *RawBus; }

private:
	TUniquePtr<rpg::core::Bus> RawBus;
};
