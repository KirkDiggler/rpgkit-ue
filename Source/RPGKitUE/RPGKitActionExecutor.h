// Plain C++ action executor shared by player cards and enemy intents.

#pragma once

#include "CoreMinimal.h"
#include "RPGKitGameMode.h"

struct FRPGKitActionExecutor
{
	bool ExecuteAction(ARPGKitGameMode& Runtime, const FRPGKitActionContext& Context, const FRPGKitCardAction& Action) const;
	FString ResolveActionTargetId(const FRPGKitCardAction& Action, const FRPGKitActionContext& Context) const;
};
