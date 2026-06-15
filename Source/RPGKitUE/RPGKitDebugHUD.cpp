#include "RPGKitDebugHUD.h"

#include "RPGKitGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

void ARPGKitDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	float Y = 40.0f;
	DrawLine(TEXT("RPGKit Debug HUD"), Y, FLinearColor::Yellow);
	DrawLine(TEXT("Keys: 1/2/3 play cards, H logs hand, E ends turn"), Y, FLinearColor::Gray);

	ARPGKitGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARPGKitGameMode>() : nullptr;
	if (!GameMode)
	{
		DrawLine(TEXT("RPGKitGameMode not active for this level."), Y, FLinearColor::Red);
		return;
	}

	const FRPGKitFighter& Hero = GameMode->GetFighter(TEXT("hero"));
	const FRPGKitFighter& Goblin = GameMode->GetFighter(TEXT("goblin"));

	Y += 8.0f;
	DrawLine(FString::Printf(TEXT("Hero: %s HP %d/%d Block %d"),
		*Hero.Name, Hero.CurrentHP, Hero.MaxHP, Hero.Block), Y, FLinearColor::Green);
	DrawLine(FString::Printf(TEXT("Enemy: %s HP %d/%d Block %d"),
		*Goblin.Name, Goblin.CurrentHP, Goblin.MaxHP, Goblin.Block), Y, FLinearColor::Red);
	DrawLine(FString::Printf(TEXT("Energy: %d/%d  Turn: %d"),
		GameMode->CurrentEnergy, GameMode->MaxEnergy, GameMode->GetTurnNumber()), Y, FLinearColor::White);

	Y += 8.0f;
	DrawLine(TEXT("Current Hand:"), Y, FLinearColor::Yellow);
	for (int32 Index = 0; Index < GameMode->CurrentHand.Num(); ++Index)
	{
		DrawLine(GameMode->GetHandCardSummary(Index), Y, FLinearColor::White);
	}

	Y += 8.0f;
	DrawLine(TEXT("Combat Log:"), Y, FLinearColor::Yellow);
	for (const FString& Message : GameMode->GetRecentCombatLog())
	{
		DrawLine(Message, Y, FLinearColor::Gray);
	}

	Y += 8.0f;
	DrawLine(TEXT("Latest Damage Chain:"), Y, FLinearColor::Yellow);
	if (GameMode->GetLatestDamageBreakdown().Num() == 0)
	{
		DrawLine(TEXT("<no modifiers>"), Y, FLinearColor::Gray);
	}
	else
	{
		for (int32 Index = 0; Index < GameMode->GetLatestDamageBreakdown().Num(); ++Index)
		{
			DrawLine(GameMode->GetDamageBreakdownSummary(Index), Y, FLinearColor::Gray);
		}
	}
}

void ARPGKitDebugHUD::DrawLine(const FString& Text, float& Y, const FLinearColor& Color)
{
	DrawText(Text, Color, 40.0f, Y, nullptr, 1.15f, false);
	Y += 22.0f;
}
