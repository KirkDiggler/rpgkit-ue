// rpgkit UE — Effect implementations

#include "RPGKitEffect.h"
#include "RPGKitBus.h"

#include "rpg/core/bus.hpp"
#include "rpg/core/chain.hpp"
#include "rpg/core/effect.hpp"
#include "rpg/core/topic.hpp"

// =========================================================================
//  Tough Skin inner effect
// =========================================================================

class URPGKitToughSkinEffect::FToughSkinEffect : public rpg::core::Effect
{
public:
	URPGKitToughSkinEffect& Owner;

	explicit FToughSkinEffect(URPGKitToughSkinEffect& InOwner)
		: rpg::core::Effect(
			TCHAR_TO_UTF8(*FString::Printf(TEXT("toughskin-%s"), *InOwner.ProtectedEntityId)),
			"tough-skin")
		, Owner(InOwner)
	{
	}

	rpg::core::Status onApply(rpg::core::Bus& bus) override
	{
		rpg::core::ChainedTopic<FRPGKitDamageEvent> topic =
			RPGKitTopics::kCombatDamage.onChained(bus);

		auto id = topic.subscribe([this](const FRPGKitDamageEvent& event,
										 rpg::core::Chain<FRPGKitDamageEvent>& chain) -> rpg::core::Status {
			// Only reduce damage against the protected entity.
			if (event.TargetId != TCHAR_TO_UTF8(*Owner.ProtectedEntityId))
			{
				return rpg::core::Status::ok();
			}

			const int32 reduction = Owner.DamageReduction;
			return chain.add("effects",
				TCHAR_TO_UTF8(*FString::Printf(TEXT("tough-skin-%s"), *Owner.ProtectedEntityId)),
				[reduction](FRPGKitDamageEvent e) -> FRPGKitDamageEvent {
					e.BaseAmount = FMath::Max(0, e.BaseAmount - reduction);
					return e;
				});
		});

		track(id);
		return rpg::core::Status::ok();
	}
};

URPGKitToughSkinEffect::URPGKitToughSkinEffect()
{
	InnerEffectImpl = new FToughSkinEffect(*this);
	RawEffectPtr = InnerEffectImpl;
}

URPGKitToughSkinEffect::~URPGKitToughSkinEffect()
{
	delete InnerEffectImpl;
	InnerEffectImpl = nullptr;
}

// =========================================================================
//  Bleed inner effect
// =========================================================================

class URPGKitBleedEffect::FBleedEffect : public rpg::core::Effect
{
public:
	URPGKitBleedEffect& Owner;

	explicit FBleedEffect(URPGKitBleedEffect& InOwner)
		: rpg::core::Effect(
			TCHAR_TO_UTF8(*FString::Printf(TEXT("bleed-%s"), *InOwner.TargetEntityId)),
			"rend")
		, Owner(InOwner)
	{
	}

	rpg::core::Status onApply(rpg::core::Bus& bus) override
	{
		rpg::core::Topic<int32> topic =
			RPGKitTopics::kTurnEnded.on(bus);

		rpg::core::Bus* BusPtr = &bus;
		auto id = topic.subscribe([this, BusPtr](const int32& turnNumber) -> rpg::core::Status {
			(void)turnNumber;

			if (Owner.Stacks <= 0)
			{
				return rpg::core::Status::ok();
			}

			// Request bleed damage. The encounter runtime owns HP mutation.
			const int32 dmg = Owner.Stacks * Owner.DamagePerStack;

			FRPGKitRawDamageRequest Request;
			Request.SourceId = TEXT("bleed");
			Request.TargetId = Owner.TargetEntityId;
			Request.Amount = dmg;

			rpg::core::Topic<FRPGKitRawDamageRequest> rawDamageTopic =
				RPGKitTopics::kRawDamageRequested.on(*BusPtr);
			rpg::core::Status published = rawDamageTopic.publish(Request);
			if (!published.isOk())
			{
				return published;
			}

			// Update state (Blueprint can see Stacks change via UPROPERTY).
			Owner.Stacks = FMath::Max(0, Owner.Stacks - 1);
			Owner.OnBleedTicked(Owner.Stacks, dmg);

			return rpg::core::Status::ok();
		});

		track(id);
		return rpg::core::Status::ok();
	}
};

URPGKitBleedEffect::URPGKitBleedEffect()
{
	InnerEffectImpl = new FBleedEffect(*this);
	RawEffectPtr = InnerEffectImpl;
}

URPGKitBleedEffect::~URPGKitBleedEffect()
{
	delete InnerEffectImpl;
	InnerEffectImpl = nullptr;
}

// =========================================================================
//  Vulnerable inner effect
// =========================================================================

class URPGKitVulnerableEffect::FVulnerableEffect : public rpg::core::Effect
{
public:
	URPGKitVulnerableEffect& Owner;

	explicit FVulnerableEffect(URPGKitVulnerableEffect& InOwner)
		: rpg::core::Effect(
			TCHAR_TO_UTF8(*FString::Printf(TEXT("vulnerable-%s"), *InOwner.TargetEntityId)),
			"vulnerable")
		, Owner(InOwner)
	{
	}

	rpg::core::Status onApply(rpg::core::Bus& bus) override
	{
		rpg::core::ChainedTopic<FRPGKitDamageEvent> damageTopic =
			RPGKitTopics::kCombatDamage.onChained(bus);

		auto damageId = damageTopic.subscribe([this](const FRPGKitDamageEvent& event,
												 rpg::core::Chain<FRPGKitDamageEvent>& chain) -> rpg::core::Status {
			if (Owner.RemainingTurns <= 0 || event.TargetId != TCHAR_TO_UTF8(*Owner.TargetEntityId))
			{
				return rpg::core::Status::ok();
			}

			const int32 PercentBonus = Owner.PercentBonus;
			return chain.add("effects",
				TCHAR_TO_UTF8(*FString::Printf(TEXT("vulnerable-%s"), *Owner.TargetEntityId)),
				[PercentBonus](FRPGKitDamageEvent e) -> FRPGKitDamageEvent {
					e.BaseAmount = FMath::CeilToInt(static_cast<float>(e.BaseAmount) * (1.0f + static_cast<float>(PercentBonus) / 100.0f));
					return e;
				});
		});

		track(damageId);

		rpg::core::Topic<int32> turnTopic = RPGKitTopics::kTurnEnded.on(bus);
		auto turnId = turnTopic.subscribe([this](const int32& turnNumber) -> rpg::core::Status {
			(void)turnNumber;

			if (Owner.RemainingTurns > 0)
			{
				Owner.RemainingTurns = FMath::Max(0, Owner.RemainingTurns - 1);
				Owner.OnVulnerableTicked(Owner.RemainingTurns);
				UE_LOG(LogTemp, Log, TEXT("RPGKit: Vulnerable on %s ticks to %d turns remaining"),
					*Owner.TargetEntityId, Owner.RemainingTurns);
			}

			return rpg::core::Status::ok();
		});

		track(turnId);

		return rpg::core::Status::ok();
	}
};

URPGKitVulnerableEffect::URPGKitVulnerableEffect()
{
	InnerEffectImpl = new FVulnerableEffect(*this);
	RawEffectPtr = InnerEffectImpl;
}

URPGKitVulnerableEffect::~URPGKitVulnerableEffect()
{
	delete InnerEffectImpl;
	InnerEffectImpl = nullptr;
}
