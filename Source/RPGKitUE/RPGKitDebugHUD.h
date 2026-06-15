#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RPGKitDebugHUD.generated.h"

UCLASS()
class ARPGKitDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLine(const FString& Text, float& Y, const FLinearColor& Color = FLinearColor::White);
};
