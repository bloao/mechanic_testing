#include "CompanionGameInstance.h"

#include "CompanionGameMode.h"
#include "Engine/World.h"
#include "Templates/UnrealTemplate.h"

AGameModeBase* UCompanionGameInstance::CreateGameModeForURL(FURL InURL, UWorld* InWorld)
{
	// Use the full package URL here: OverrideGameModeClass receives only a short
	// name, which cannot distinguish project maps from Engine utility worlds.
	const bool bProjectMap = InURL.Map.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase);
	TGuardValue<bool> SelectionScope(bCreatingCompanionGameplayMode, bProjectMap);
	return Super::CreateGameModeForURL(InURL, InWorld);
}

TSubclassOf<AGameModeBase> UCompanionGameInstance::OverrideGameModeClass(
	TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const
{
	if (bCreatingCompanionGameplayMode)
	{
		// Every project map, including new copies and PIE instances, receives the
		// shared playable characters, abilities, AI controller and navigation setup.
		return ACompanionGameMode::StaticClass();
	}
	return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);
}
