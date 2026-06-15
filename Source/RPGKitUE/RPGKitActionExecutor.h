// Plain C++ action executor shared by player cards and enemy intents.

#pragma once

#include "CoreMinimal.h"

class ARPGKitGameMode;
struct FRPGKitActionContext;
struct FRPGKitCardAction;

struct FRPGKitActionExecutor
{
	bool ExecuteAction(ARPGKitGameMode& Runtime, const FRPGKitActionContext& Context, const FRPGKitCardAction& Action) const;
	FString ResolveActionTargetId(const FRPGKitCardAction& Action, const FRPGKitActionContext& Context) const;
};
